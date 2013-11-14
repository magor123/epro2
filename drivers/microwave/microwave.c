/*
	HALOGEN lights driver a.k.a "microwave"

	This driver is only a combination of the three drivers found in the "templates" folder of this project.

	Functionality:
	 - storing an intiger (string) to internal buffer;
	 - setting up 3 GPIO pins (as outputs).

	Configuration:
	 - 3 halogen lights connected to pins 12, 14 and 16 of header P9.
        
	Usage sequence:
	 - when inserted, the driver configures the GPIOs;
	 - user can change the heating by writing in /dev/microwave how many halogen lights are to be turned on;
	 - when removed, the driver turns off the halogen lights and releases the GPIOs.
*/


#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/types.h>        					// dev_t data type
#include <linux/kdev_t.h>       					// dev_t: Major() and Minor() functions
#include <linux/fs.h>           					// chrdev regirstration: alloc_chardev_region()
#include <linux/device.h>       					// udev support: class_create() and device_create()
#include <linux/cdev.h>         					// VFS registration: cdev_init() and cdev_add()
#include <linux/uaccess.h>      					// copy_to_user() and read_from_user()
#include <linux/gpio.h>         					// gpio kernel library

#define MSG_SIZE 3							// my buffer size

static dev_t devnum;            					// my dynamically allocated device number <Major,Minor>
static char msg[MSG_SIZE];						// my buffer to store a string
static struct cdev mydev;       					// character device structure
static struct class *cl;        					// device class
static int hal1_gpio = 44;      					// first halogen gpio number (P9_12 --> gpio1[28])
static int hal2_gpio = 26;      					// second halogen gpio number (P9_14 --> gpio1[18])
static int hal3_gpio = 46;      					// third halogen gpio number (P9_16 --> gpio1[19])
static int hal_status = 0;      					// halogen status memory (0 = OFF, 1 = one ON, 2 = two ON, 3 = all three ON)

	// READ // report the current halogen lights status to the user

static ssize_t my_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	snprintf(msg, MSG_SIZE, "%i\n", hal_status);			// homemade integer to string conversion

	if (*off >= MSG_SIZE) {return 0;}				// no more (or nothing) to read in my buffer
	if (*off + len > MSG_SIZE) {len = MSG_SIZE - *off;}		// prevent overflow reading my buffer
	if (copy_to_user(buf, &msg + *off, len) != 0){return -EFAULT;}	// read LEN bytes from my buffer

	*off += len;							// increment offset
	return len;
};

	// WRITE // get the new halogen lights status from the user

static ssize_t my_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	if (*off >= MSG_SIZE) return -ENOSPC;				// no more space left to write in my buffer
	if (*off + len >= MSG_SIZE) {return -EINVAL;}			// get hal_status from user, return an error for numbers bigger than 1 digit (+'/0')
	if (copy_from_user(&msg + *off, buf, len)!= 0){return -EFAULT;}	// extract a chunk of data (of LEN bytes) from the user buffer and store it in my buffer at 										position OFFSET

	sscanf(msg,"%i",&hal_status);					// homemade string to integer conversion

	if     (hal_status == 1){					// turn on the appropriate light according to the set value hal_status (one)
		gpio_set_value(hal1_gpio, 1);
		gpio_set_value(hal2_gpio, 0);
		gpio_set_value(hal3_gpio, 0);}
	else if(hal_status == 2){					// two lights, if number two is written
		gpio_set_value(hal1_gpio, 1);
		gpio_set_value(hal2_gpio, 1);
		gpio_set_value(hal3_gpio, 0);}
	else if(hal_status == 3){					// turn on all the lights, when digit "3" is recieved
		gpio_set_value(hal1_gpio, 1);
		gpio_set_value(hal2_gpio, 1);
		gpio_set_value(hal3_gpio, 1);}
	else   {gpio_set_value(hal1_gpio, 0);				// when other [!= {1,2,3}] inputs are detected, turn off all the lights
		gpio_set_value(hal2_gpio, 0);
		gpio_set_value(hal3_gpio, 0);
		if (hal_status != 0){					// if input is none of the four predefined values [!= {0,1,2,3}], return an error as well 
			hal_status = 0;
			return -EINVAL;}}

	*off += len;							// increment offset, the placeholder inside my buffer

	return len;							// return the number of bytes I was able to write in my driver buffer a new iteration of 										this function is expected if there is still user data pending to be store in the driver
};

	// FILE OPERATIONS & FUNCTION CALLBACKS //

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.read  = my_read,
	.write = my_write
};

	// CONSTRUCTOR //

static int __init my_init(void)
{
	if (alloc_chrdev_region(&devnum, 0, 1, "microwave") < 0){
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "epro")) == NULL){
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	if (device_create(cl, NULL, devnum, NULL, "microwave") == NULL){
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	cdev_init(&mydev, &fops);
	if (cdev_add(&mydev, devnum, 1) == -1){
		device_destroy(cl, devnum);
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	printk(KERN_INFO "HALOGEN module registered, <Major, Minor>: <%d, %d>, HZ:%d \n", MAJOR(devnum), MINOR(devnum), HZ);
	
	if (!gpio_is_valid(hal1_gpio) || !gpio_is_valid(hal2_gpio) || !gpio_is_valid(hal3_gpio)){
		printk(KERN_ALERT "Microwave: One or more of the requested GPIOs for halogen lights is not valid \n");
		return -EINVAL;						// check if gpio numbers are valid on this bo
	}
	if(gpio_request_one(hal1_gpio, GPIOF_OUT_INIT_LOW,"Halogen 1")){// request gpio pins and configure directions
		printk(KERN_ALERT "Microwave: Unable to request gpio %d \n", hal1_gpio);
		return -EINVAL;
	}
	if(gpio_request_one(hal2_gpio, GPIOF_OUT_INIT_LOW, "Halogen 2")){
		printk(KERN_ALERT "Microwave: Unable to request gpio %d \n", hal2_gpio);
		return -EINVAL;
	}
	if(gpio_request_one(hal3_gpio, GPIOF_OUT_INIT_LOW, "Halogen 3")){
		printk(KERN_ALERT "Microwave: Unable to request gpio %d \n", hal3_gpio);
		return -EINVAL;
	}
	return 0;
};

	// DESTRUCTOR //

static void __exit my_exit(void)
{
	gpio_set_value(hal1_gpio, 0);					// switch off the lights
	gpio_set_value(hal2_gpio, 0);
	gpio_set_value(hal3_gpio, 0);

	gpio_free(hal1_gpio);						// release previously-claimed GPIO pins
	gpio_free(hal2_gpio);
	gpio_free(hal3_gpio);

	cdev_del(&mydev);						// unregister character device
	device_destroy(cl, devnum);
	class_destroy(cl);
	unregister_chrdev_region(devnum, 1);
	printk(KERN_INFO "HALOGEN module unregistered \n");
};

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("HALOGEN driver - you set, I cook, 29.10.2013");
MODULE_AUTHOR("Massimo Testa & Matija Goršič");
