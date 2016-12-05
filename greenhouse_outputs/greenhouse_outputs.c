#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define PIN_FAN		2
#define PIN_LIGHT	4
#define PIN_RESISTOR	5


static int device_read(struct file *f, char __user *data, size_t size, loff_t *l);
static int device_write(struct file *f, const char __user *data, size_t size, loff_t *l);
static int device_open(struct inode *i, struct file *f);
static int device_release(struct inode *i, struct file *f);
static long device_ioctl (struct file *, unsigned int, unsigned long);

static struct gpio gpio_config[] = {{PIN_FAN, GPIOF_OUT_INIT_LOW, "FAN"},		//Config GPIO pin for the FAN
				{PIN_LIGHT, GPIOF_OUT_INIT_LOW, "LIGHT"},		//Config GPIO pin for the LIGHT
				{PIN_RESISTOR, GPIOF_OUT_INIT_LOW, "RESISTOR"}};	//Config GPIO pin for the RESISTOR


struct file_operations fops = {								//DevFS unherited operations
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




/* Cette fonction est appelée quand l'utilisateur ouvre notre fichier virtuel.
 */
static int device_open(struct inode *i, struct file *f)
{
	printk(KERN_INFO"Ouverture !\n");
	return 0;
}

static int device_probe(struct spi_device *spi)
{
	printk(KERN_INFO"Probe !\n");
	return 0;
}


/* Cette fonction est appelée quand l'utilisateur effectue une lecture sur notre
 * fichier virtuel.
 * Ce driver interface des périphériques de sortie seulement, cette fonction de lecture ne retourne donc rien. 
*/
static int device_read(struct file *f, char __user *data, size_t size, loff_t *l)
{
	return 0;
}


/* Cette fonction est appelée quand l'utilisateur effectue une écriture sur
 * notre fichier virtuel.
 *
 * Elle reçoit un buffer contenant des données. 
 * Elle doit utiliser les données et retourner le nombre de données
 * effectivement utilisées.

 * Here, data must be : FAN(0 or 1)LIGHT(0 or 1)RESISTOR(0 or 1)	
*/
static int device_write(struct file *f, const char __user *data, size_t size, loff_t *l)
{
	char *plop = kmalloc(size + 1, GFP_KERNEL);
	int i;
	
	
	copy_from_user(plop, data, size);
	plop[size] = 0;
	
/*	
 *	i = 0 -> PIN_FAN is set to FAN
 *	i = 1 -> PIN_LIGHT is set to LIGHT
 *	i = 2 -> PIN_RESISTOR is set to RESISTOR
*/
	for(i = 0;i < 3;i++){
		
		
		if(plop[i] == '1'){
			gpio_set_value(gpio_config[i].gpio, 1);
		}else{
			gpio_set_value(gpio_config[i].gpio, 0);
		}
	}

	kfree(plop);
	return size;
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

/*
 * Cette fonction est appelée à l'initialisation du périphérique. 
 * Elle crée l'interface devFs et initalise les GPIOs.
*/

static int __init tst_init(void)
{
	int status;
	


	printk(KERN_INFO"Initialisation des périphériques de sortie.\n");

	major = register_chrdev(0, "greenhouse_outputs", &fops);
	if ( major < 0)
	{
		printk(KERN_INFO "Echec de register_chrdev\n");
		return major;
	}

	class = class_create(THIS_MODULE, "Greenhouse_outputs");
	if (IS_ERR(class))
	{
		printk(KERN_INFO "echec class_create\n");
		status = PTR_ERR(class);
		goto errorClass;
	}

	devt = MKDEV(major, 0);
	dev = device_create(class, NULL, devt,
	                    NULL, "greenhouse_outputs");
	status = IS_ERR(dev) ? PTR_ERR(dev) : 0;

	if (status !=0 )
	{
		printk(KERN_ERR "Erreur device create\n");Montage 1.2
		goto error;
	}

	status = gpio_request_array(gpio_config, 3);
	
	if(status != 0){
		printk(KERN_ERR "Device's PIN reservation failed\n");
		return status;

	}	
	
	
	return 0;

error:
	class_destroy(class);
errorClass:
	unregister_chrdev(major, "greenhouse_outputs");
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
	unregister_chrdev(major, "greenhouse_outputs");
	gpio_free_array(gpio_config, 3);
	printk(KERN_INFO"Goodbye world!\n");
}

module_init(tst_init);
module_exit(tst_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Maxime Normand, Maxime Hubert");
MODULE_DESCRIPTION("Greenhouse outputsv peripherals.");
