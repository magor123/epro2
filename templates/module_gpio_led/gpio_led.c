/*
	GPIO_LED driver a.k.a "gled" (or "gangsta-led")

	In this episode:
	- Setup of a GPIO pin (as output)
	- Implementation of a Timer

	Configuration: We have a led connected to pin 10 of header P8 (gpio2[4])

	Usage:
	- When inserted, the driver configures the GPIO as output and start blinking the led
	- User can change the blinking frequency by writing the milliseconds value in /dev/gled (0=OFF)
	- When removed, the driver deletes the timer, turns off the led and releases the gpio.

	Pin 10 on header P8 has been chosen since it comes configured as GPIO by default:
	Pin Multiplexing on Beaglebone is out of scope for this simple driver.
*/


#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/device.h>	// udev support: class_create() and device_create()
#include <linux/cdev.h>		// VFS registration: cdev_init() and cdev_add()
#include <linux/uaccess.h>	// copy_to_user() and read_from_user()

#include <linux/timer.h>	// timer functions
#include <linux/gpio.h>		// gpio kernel library

#define MSG_SIZE 5

static dev_t devnum; 		// my dynamically allocated device number <Major,Minor>
static struct cdev mydev;	// character device structure
static struct class *cl;	// device class

static struct timer_list my_timer;

static int led_gpio_num = 68;	// gpio number (P8_10 -> gpio2[4]), it could be nice to have it as a driver parameter
static int led_status = 0;	// 0=OFF, 1=ON
static int led_msec   = 250;	// blinking interval in milliseconds [0 = turn off and stop blinking]

static char msg[MSG_SIZE];	// my buffer needed(?) for the read and write functions


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
 *	Report to the user the current blinking interval time (0=LED_OFF)
 */
static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	// homemade integer to string conversion 
	snprintf(msg, MSG_SIZE, "%d", led_msec);
	
	// no more (or nothing) to read in my buffer
	if (*off >= MSG_SIZE) {	return 0; }
	// prevent overflow reading my buffer
	if (*off + len > MSG_SIZE) { len = MSG_SIZE - *off; }
	// read LEN bytes from my buffer
	if (copy_to_user(buf, &msg + *off, len) != 0) { return -EFAULT; }

	*off += len;
	return len;
}


/*
 *	WRITE:
 *	Get the new blinking interval time from the user
 */
static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	// get msec from user, return an error for numbers bigger than 4 digits (+'/0')
	if (*off + len > MSG_SIZE) { return -EINVAL; }
	if ( copy_from_user(&msg + *off, buf, len) != 0 ) { return -EFAULT; }
	*off += len;

	// homemade string to integer conversion
	sscanf(msg,"%d",&led_msec);	//maybe kstrtoul(msg,len,&led_msec)? or simple_strtoul()?

	// if led is OFF and user set a value greater then zero: start blinking
	if( (led_status==0) && (led_msec!=0)) {
		mod_timer( &my_timer, jiffies + msecs_to_jiffies(led_msec) );
	}

	return len;
}


/*
 *	CALLBACK FUNCTION FOR THE TIMER
 */
static void toggle_led(unsigned long data)
{
	// toggle led
	if(led_status==0) led_status=1; else led_status=0;
	gpio_set_value(led_gpio_num, led_status);		

	// set a new timer expiration (keep blinking) or switch off the led
	if (led_msec!=0) {
		mod_timer( &my_timer, jiffies + msecs_to_jiffies(led_msec) );
	}else {
		led_status=0;
		gpio_set_value(led_gpio_num, led_status);
	}
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
	// register character device as usual
	if (alloc_chrdev_region(&devnum, 0, 1, "gled") < 0) {
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "epro")) == NULL) {
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	if (device_create(cl, NULL, devnum, NULL, "gled") == NULL){
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
	printk(KERN_INFO "GPIO_LED registered, <Major, Minor>: <%d, %d>, HZ:%d \n", MAJOR(devnum), MINOR(devnum), HZ);


	// check if gpio number is valid on this board
	if (!gpio_is_valid(led_gpio_num)){
		printk(KERN_ALERT "The requested GPIO is not available \n");
		return -EINVAL;
	}

	// allocate the gpio
	if(gpio_request(led_gpio_num, "blinking_led")){
		printk(KERN_ALERT "Unable to request gpio %d \n", led_gpio_num);
		return -EINVAL;
	}

	// configure the gpio as output, initially OFF
	if( gpio_direction_output(led_gpio_num, led_status) < 0 ){
		printk(KERN_ALERT "Impossible to set output direction \n");
		return -EINVAL;
	}
	
	// setup the timer
	setup_timer( &my_timer, toggle_led, 0 );

	// (optional) start blinking with default blinking interval
	mod_timer( &my_timer, jiffies + msecs_to_jiffies(led_msec) );

	return 0;
}

/*
 *	DECONSTRUCTOR
 */
static void __exit my_exit(void)
{
	// remove timer
	del_timer( &my_timer);

	// switch led off
	gpio_set_value(led_gpio_num, 0);
	
	// release previously-claimed GPIO 
	gpio_free(led_gpio_num);

	// unregister character device
	cdev_del(&mydev);
	device_destroy(cl, devnum);
	class_destroy(cl);
	unregister_chrdev_region(devnum, 1);
	printk(KERN_INFO "GPIO_LED unregistered \n");
}

module_init(my_init);
module_exit(my_exit);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("GPIO_LED driver - let's blink that led, Oct. 2013");
MODULE_AUTHOR("Massimo Testa");
