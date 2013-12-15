/*
	PWM driver to control a fan (or fade a led, as you prefer)


	Configuration:
	- We have a led/fan connected to pin 13 of connector P8 (ehrpwm2B)

	Usage:
	- write in /dev/eprofan the desired fan speed in percentage [0-100]
	- the driver will gently ramp up(down) the fan speed to the desired percentage
	- have fun

*/


#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>

#include <linux/types.h>		// dev_t data type
#include <linux/kdev_t.h>		// dev_t: Major() and Minor() functions
#include <linux/fs.h>			// chrdev regirstration: alloc_chardev_region()
#include <linux/device.h>		// udev support: class_create() and device_create()
#include <linux/cdev.h>			// VFS registration: cdev_init() and cdev_add()
#include <linux/uaccess.h>		// copy_to_user() and read_from_user()

#include <linux/timer.h>		// timer functions

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/pwm.h>

#define FAN_PERIOD	50000		// [nanoseconds] 20kHz frequency
#define DUTY_INCREMENT	  500		// [nanoseconds] 1% of the period
#define FAN_STEP	   50		// [milliseconds] interval in beetewen duty increments

static dev_t devnum; 			// my dynamically allocated device number <Major,Minor>
static struct cdev mydev;		// character device structure
static struct class *cl;		// device class

static struct timer_list fan_timer;
static int fan_percentage = 0;		// fan speed percentage
static int fan_duty 	  = 0;		// fan duty cycle

struct pwm_device *pwm_a;		// PWM device



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
	char tmp[4];

	if ( copy_from_user(&tmp, buf, len) != 0 ) { return -EFAULT; }
	//if ( kstrtoint(tmp,10,&fan_percentage) )   { return -EINVAL; }
	fan_percentage=simple_strtol(tmp,NULL,0);
	
	printk(KERN_INFO "PWM_A, FAN PERCENTAGE: rc=[%d] \n", fan_percentage);
	if(fan_percentage > 100) { fan_percentage=100; return -EINVAL;}
	if(fan_percentage < 0)   { fan_percentage=0;   return -EINVAL;}

	// lux fiat
	mod_timer( &fan_timer, jiffies + msecs_to_jiffies(FAN_STEP) );

	return len;
}


/*
 *	CALLBACK FUNCTION FOR THE TIMER
 */
static void adjust_speed(unsigned long data)
{

	int target_duty = fan_percentage * DUTY_INCREMENT;		

	// once you get to ther desired duty cycle, exit the loop	
	if (fan_duty==target_duty) return;

	// increment or decremente the duty cycle by 1%
	if(fan_duty<target_duty) { fan_duty+=DUTY_INCREMENT; }
	else { fan_duty-=DUTY_INCREMENT; }
	
	// apply new configuration to PWM device
	pwm_config(pwm_a, fan_duty, FAN_PERIOD);
	
	// reschedule next speed adjustment
	mod_timer( &fan_timer, jiffies + msecs_to_jiffies(FAN_STEP) );
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
	if (alloc_chrdev_region(&devnum, 0, 1, "eprofan") < 0) {
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "fans")) == NULL) {
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	if (device_create(cl, NULL, devnum, NULL, "eprofan") == NULL){
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
	printk(KERN_INFO "PWM module registered, <Major, Minor>: <%d, %d>\n", MAJOR(devnum), MINOR(devnum));



	/*
	 *	REQUEST A PWM CHANNEL
	 *
	 *	0: 48300200.ehrpwm, pwm-0	-> [ehrpwm0A] P9_22
	 *	1: 48300200.ehrpwm, pwm-1	-> [ehrpwm0B] P9_21
	 *
	 *	2: 48300100.ecap,   pwm-0	-> ?
	 *
	 *	3: 48302200.ehrpwm, pwm-0	-> [ehrpwm1A] P9_14
	 *	4: 48302200.ehrpwm, pwm-1	-> [ehrpwm1B] P9_16
	 *
	 *	5: 48304200.ehrpwm, pwm-0	-> [ehrpwm2A] P8_19
	 *	6: 48304200.ehrpwm, pwm-1	-> [ehrpwm2B] P8_13
	 *
	 *	7: 48304100.ecap,   pwm-0	-> ?
	 */
	pwm_a=pwm_request(6,"EPRO_FAN_1");

	// Configure PWM device (initially OFF)
	fan_duty=0;
	rc = pwm_config(pwm_a, fan_duty, FAN_PERIOD);
	printk(KERN_INFO "PWM_A, pwm_config(): rc=[%d] \n", rc);

	// Enable PWM device
	rc = pwm_enable(pwm_a);
	printk(KERN_INFO "PWM_A, pwm_enable(): rc=[%d] \n", rc);
	
	// setup the timer
	setup_timer( &fan_timer, adjust_speed, 0 );


	return 0;
}

/*
 *	DECONSTRUCTOR
 */
static void __exit my_exit(void)
{
	// remove timer
	del_timer(&fan_timer);

	pwm_disable(pwm_a);
	printk(KERN_INFO "PWM_A, pwm_disable()\n");

	pwm_free(pwm_a);
	printk(KERN_INFO "PWM_A, pwm_free()\n");

	// unregister character device
	cdev_del(&mydev);
	device_destroy(cl, devnum);
	class_destroy(cl);
	unregister_chrdev_region(devnum, 1);
	printk(KERN_INFO "PWM module unregistered \n");
}

module_init(my_init);
module_exit(my_exit);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PWM driver - no matter if u spinnin' a fan or fading a led, do it with style. Oct. 2013");
MODULE_AUTHOR("Massimo Testa <mates12[at]student[dot]sdu[dot]dk>");
