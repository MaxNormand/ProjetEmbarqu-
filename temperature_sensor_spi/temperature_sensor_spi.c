#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>

#include <linux/gpio.h>

#define PIN_LIGHT_STATUS 	1

static int device_read(struct file *f, char __user *data, size_t size, loff_t *l);
static int device_write(struct file *f, const char __user *data, size_t size, loff_t *l);
static int device_open(struct inode *i, struct file *f);
static int device_release(struct inode *i, struct file *f);
static long device_ioctl (struct file *, unsigned int, unsigned long);


struct file_operations fops = {						//DevFS unherited operations
	.read = device_read,
	.write = device_write,
	.unlocked_ioctl = device_ioctl,
	.open = device_open,
	.release = device_release,
};


int major;
struct device *dev;
static struct class *class;
dev_t devt;

struct spi_device *myspi;						// SPI objects to control the ADC for temperature
static DEFINE_SPINLOCK(spilock);
static unsigned long spilockflags = 0;

static struct gpio gpio_light_status[] = {{PIN_LIGHT_STATUS, 		//Config GPIO pin for the light sensor
					GPIOF_IN,
					"LIGHT_STATUS"}};


static unsigned spi_temperature_read(struct spi_device * spi){

	struct spi_transfer rx;
	struct spi_message m;

	char dst[4] = {0, 0, 0, 0};

	
	unsigned mask = 0x1FFE0000; 					// Mask to read from the 4th to the 12th bit
	unsigned tmp = 0;
	int i;
	

	spi_message_init(&m);

	memset(&rx, 0, sizeof(rx));					// Reads the value from the SPI and stores it in dst
	rx.rx_buf = dst;
	rx.len = sizeof(dst);
	
	spi_message_add_tail(&rx, &m);
	spin_lock_irqsave(&spilock, spilockflags);
	if(spi_sync(myspi, &m) < 0)					// Checks the status message from the SPI device
		printk(KERN_ERR"Erreur sync\n");
	
	spin_unlock_irqrestore(&spilock, spilockflags);
	
	
	for(i = 0; i < 3; i++){					//  dst[0]	[********]
		tmp += dst[i];					// +dst[1] 			[********]
		tmp <<= 8;					// +dst[2]					[********]
	}							// +dst[3]							[********]
								// =
								// tmp		[********	********	********	********]
	tmp += dst[3];						// 
	tmp = mask & tmp;					// mask		[00011111	11111110	00000000	00000000]
								// & tmp =	[000*****	*******0	00000000	00000000]

	tmp >>= 17;

								// tmp = 	[00000000	00000000	0000****	********]
	return tmp;			

}



/* Cette fonction est appelée quand l'utilisateur ouvre notre fichier virtuel.
 */
static int device_open(struct inode *i, struct file *f)
{	
	printk(KERN_INFO"Ouverture!\n");
	return 0;
}


/* Cette fonction est appelée quand l'utilisateur effectue une lecture sur notre
 * fichier virtuel.
 *
 * Elle reçoit un buffer à remplir et une taille demandée.
 * Elle doit remplir le buffer et retourner la taille effective.
*/
static int device_read(struct file *f, char __user *data, size_t size, loff_t *l)
{
	char buffer[32];
	
	sprintf(buffer, "%d \t %d\n", spi_temperature_read(myspi), gpio_get_value(PIN_LIGHT_STATUS));
	copy_to_user(data, buffer, 32);

	return 11;

}




/* Cette fonction est appelée quand l'utilisateur effectue une écriture sur
 * notre fichier virtuel.
 * Ce driver interface des périphériques d'entrée seulement, cette fonction de lecture ne retourne donc rien. 
*/
static int device_write(struct file *f, const char __user *data, size_t size, loff_t *l)
{
	
	return 0;
}

/* Cette fonction est appelée quand l'utilisateur ferme notre fichier virtuel.
 */
static int device_release(struct inode *i, struct file *f)
{
	printk(KERN_INFO"Fermeture!\n");
	return 0;
}

/* Cette fonction est appelée quand l'utilisateur effectue un ioctl sur notre
 * fichier.
 */
static long device_ioctl (struct file *f, unsigned int cmd, unsigned long arg)
{
	printk(KERN_INFO"ioctl!\n");
	return 0;
}

static int temperature_probe(struct spi_device *spi)
{
	printk(KERN_INFO"Temperature Probe !\n");
	myspi = spi;
	return 0;
}

static int temperature_remove(struct spi_device *spi)
{
	printk(KERN_INFO"Remove\n");
	return 0;
}

static struct spi_driver temperature_sensor_driver = {
	.driver = {
		.name = "temperature", // be sure to match this with the spi_board_info modalias in arch/am/mach-omap2/board-am335xevm.c
		.owner = THIS_MODULE
	},
	.probe = temperature_probe,
    .remove = temperature_remove
};

/*
 * Cette fonction est appelée à l'initialisation du périphérique. 
 * Elle crée l'interface devFs et initalise les GPIOs.
*/

static int __init tst_init(void)
{
	int status;

	printk(KERN_INFO"Hello world!\n");

	major = register_chrdev(0, "temperature_sensor_spi", &fops);
	if ( major < 0)
	{
		printk(KERN_INFO "Echec de register_chrdev\n");
		return major;
	}

	class = class_create(THIS_MODULE, "temperature");
	if (IS_ERR(class))
	{
		printk(KERN_INFO "echec class_create\n");
		status = PTR_ERR(class);
		goto errorClass;
	}

	devt = MKDEV(major, 0);
	dev = device_create(class, NULL, devt,
	                    NULL, "temperature");
	status = IS_ERR(dev) ? PTR_ERR(dev) : 0;

	if (status !=0 )
	{
		printk(KERN_ERR "Erreur device create\n");
		goto error;
	}

	// registering SPI driver,

	status = spi_register_driver(&temperature_sensor_driver);

	if (status < 0) {
		printk(KERN_ERR "spi_register_driver() failed : %d\n", status);
		return status;
	}

	// registering light pin,

	status = gpio_request_array(gpio_light_status, 1);

	if (status < 0) {
		printk(KERN_ERR "gpio_light_status pin reservation failed : %d\n", status);
		return status;
	}	

	return 0;

error:
	class_destroy(class);
errorClass:
	unregister_chrdev(major, "hello");
	return status;
}

/*
 * Cette fonction supprime l'interface du driver
 * Et débranche les PINs des périphériques

*/
static void __exit tst_exit(void)
{
	device_destroy(class, devt);
	class_destroy(class);
	unregister_chrdev(major, "temperature_sensor_spi");
	spi_unregister_driver(&temperature_sensor_driver);
	gpio_free_array(gpio_light_status, 1);
	printk(KERN_INFO"Goodbye world!\n");
}

module_init(tst_init);
module_exit(tst_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxime Normand, Maxime Hubert");
MODULE_DESCRIPTION("temperature_sensor_spi");
