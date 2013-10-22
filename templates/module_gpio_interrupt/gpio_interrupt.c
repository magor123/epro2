/*
	GPIO_INTERRUPT driver a.k.a "gint" 

	In this episode:
	- Setup of GPIO pins (as input and output)
	- Registering and Handling Interrupts

	Configuration:
	- We have a led connected to pin 10 of header P8 (gpio2[4])
	- We hava a button connected to pin 12 of header P9 (gpio1[28])

	Usage:
	- When inserted, the driver configures the GPIOs and claims an interrupt line for the button pin
	- When the button is pressed (on rising edge) a callback function is executed to toggle the led
	- When removed, the driver turns off the led, frees the interrupt line and releases the gpios.


	Note: file operations OPEN, CLOSE, READ and WRITE are reported only for convinience in expanding
	the driver functionality
*/


#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>

#include <linux/types.h>	// dev_t data type
#include <linux/kdev_t.h>	// dev_t: Major() and Minor() functions
#include <linux/fs.h>		// chrdev regirstration: alloc_chardev_region()
#include <linux/device.h>	// udev support: class_create() and device_create()
#include <linux/cdev.h>		// VFS registration: cdev_init() and cdev_add()
#include <linux/uaccess.h>	// copy_to_user() and read_from_user()

#include <linux/gpio.h>		// gpio kernel library
#include <linux/interrupt.h>	// interrupt functions

static dev_t devnum; 		// my dynamically allocated device number <Major,Minor>
static struct cdev mydev;	// character device structure
static struct class *cl;	// device class

static int led_gpio_num = 68;	// LED gpio number (P8_10 -> gpio2[4])
static int btn_gpio_num = 60;	// BUTTON gpio number (P9_12 -> gpio1[28])

static int led_status = 0;	// led status memory: 0=OFF, 1=ON
static int my_irq;		// IRQ identifier



/*
 *	OPEN
 */
static int my_open(struct inode *i, struct file *f)
{
	// do nothing
	return 0;
}

/*
 *	CLOSE
 */
static int my_close(struct inode *i, struct file *f)
{
	// do nothing
	return 0;
}

/*
 *	READ:
 */
static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	// do nothing
	return 0;
}


/*
 *	WRITE:
 */
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	// do nothing
	return 0;
}


/*
 *	CALLBACK FUNCTION to handle interrupts
 */
static irqreturn_t irqhandler(int irq, void* dev_id)
{
	// toggle led
	if(led_status==0) led_status=1; else led_status=0;
	gpio_set_value(led_gpio_num, led_status);
	
	return IRQ_HANDLED;	
}


/*
 *	File operations and function callbacks
 */
static struct file_operations fops =
{
	.owner   = THIS_MODULE,
	.open    = my_open,
	.release = my_close,
	.read    = my_read,
	.write   = my_write
};



/*
 *	CONSTRUCTOR
 */
static int __init my_init(void)
{
	int rc;

	// register character device as usual
	if (alloc_chrdev_region(&devnum, 0, 1, "gint") < 0) {
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "epro")) == NULL) {
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	if (device_create(cl, NULL, devnum, NULL, "gint") == NULL){
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	cdev_init(&mydev, &fops);
	if (cdev_add(&mydev, devnum, 1) == -1)
	{
		device_destroy(cl, devnum);
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	printk(KERN_INFO "GPIO_INTERRUPT module registered, <Major, Minor>: <%d, %d>, HZ:%d \n", MAJOR(devnum), MINOR(devnum), HZ);


	// check if gpio numbers are valid on this board
	if (!gpio_is_valid(led_gpio_num)){
		printk(KERN_ALERT "GINT: The requested LED GPIO is not available \n"); return -EINVAL;
	}
	if (!gpio_is_valid(btn_gpio_num)){
		printk(KERN_ALERT "GINT: The requested BUTTON GPIO is not available \n"); return -EINVAL;
	}


	// request gpio pins and configure directions
	if(gpio_request_one(led_gpio_num, GPIOF_OUT_INIT_LOW, "blinking_led")){
		printk(KERN_ALERT "GINT: Unable to request gpio %d \n", led_gpio_num);
		return -EINVAL;
	}
	if(gpio_request_one(btn_gpio_num, GPIOF_IN, "interrupt_button")){
		printk(KERN_ALERT "GINT: Unable to request gpio %d \n", btn_gpio_num);
		gpio_free(led_gpio_num);
		return -EINVAL;
	}


	

	// Interrupt: Map gpio number to corresponding IRQ number
	my_irq = gpio_to_irq(btn_gpio_num);

	/*
	 * 	REQUEST INTERRUPT LINE
	 *
	 *	generally, it is not a good idea to request an interrupt line in the initialization
	 *	function if it is not "shared". Doing this will prevent any other device to request
	 *	the same interrupt line. It should be better to request the irq at device OPEN
	 *
	 *	Flags (can be concatenated using '|', more in source/include/linux/interrupt.h):
	 * 	IRQF_SHARED 		: allow sharing the irq among several devices
	 *	IRQF_TRIGGER_RISING	: triggered on rising edge
	 * 	IRQF_TRIGGER_FALLING   	: triggered on falling edge
	 * 	IRQF_TRIGGER_HIGH 	: triggered when high
	 * 	IRQF_TRIGGER_LOW	: triggered when low
	 */
	rc = request_irq(
			my_irq,			// interrupt line number requested
			irqhandler,		// my callback function to handle the interrupts
			IRQF_TRIGGER_RISING,	// bitmask, interrupt flags 
			"GINT",			// name of the device claiming the irq
			NULL			// struct passed to the handler function
		);

	if( rc < 0 ) {
		printk(KERN_ALERT "GINT: Impossible to request an IRQ\n");
		gpio_free(led_gpio_num);
		gpio_free(btn_gpio_num);
		return -EINVAL;
	}


	return 0;
}

/*
 *	DECONSTRUCTOR
 */
static void __exit my_exit(void)
{

	// switch led off
	gpio_set_value(led_gpio_num, 0);

	// free IRQ
	free_irq(my_irq, NULL);
	
	// release previously-claimed GPIO pins
	gpio_free(led_gpio_num);
	gpio_free(btn_gpio_num);

	// unregister character device
	cdev_del(&mydev);
	device_destroy(cl, devnum);
	class_destroy(cl);
	unregister_chrdev_region(devnum, 1);
	printk(KERN_INFO "GPIO_INTERRUPT module unregistered \n");
}

module_init(my_init);
module_exit(my_exit);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO_INTERRUPT driver - you click I blink, Oct. 2013");
MODULE_AUTHOR("Massimo Testa");
