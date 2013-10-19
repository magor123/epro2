/*
	Echobox, a Simple Linux Character Device Driver
	
	Echobox device can store a string in an internal buffer and then print it back.
	The device Major number is assigned dynamically via alloc_chrdev_region().
	Devices files are automatically created by udev deamon, no need to issue mknod commands.
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

#define MSG_SIZE 5		// my buffer size
 
static dev_t devnum; 		// my dynamically allocated device number <Major,Minor>
static struct cdev mydev;	// character device structure
static struct class *cl;	// device class
static char msg[MSG_SIZE];	// my buffer to store a string


/*
 *	OPEN
 */
static int echobox_open(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Echobox: open()\n");
	return 0;
}

/*
 *	CLOSE
 */
static int echobox_close(struct inode *i, struct file *f)
{
	printk(KERN_INFO "Echobox: close()\n");
	return 0;
}

/*
 *	READ
 */
static ssize_t echobox_read(struct file *f, char __user *buf, size_t len, loff_t *off)
{
	// no more (or nothing) to read in my buffer
	if (*off >= MSG_SIZE) {	return 0; }

	// prevent overflow reading my buffer
	if (*off + len > MSG_SIZE) { len = MSG_SIZE - *off; }

	// read LEN bytes from my buffer
	if (copy_to_user(buf, &msg + *off, len) != 0) { return -EFAULT; }

	//increment offset
	*off += len;

	return len;
}

/*
 *	WRITE
 */
static ssize_t echobox_write(struct file *f, const char __user *buf, size_t len, loff_t *off)
{
	// no more space left to write in my buffer (returns: "No space left on device" error)
	// this check is meaningful only starting from the second iteration of the write function
	if (*off >= MSG_SIZE) return -ENOSPC;
	
	// prevent overflow when writing in my buffer:
	// if the lenght of the message the user wants to write in my driver exceeds my buffer lenght
	// then I decrease the lenght of the message (LEN) to be stored in my buffer
	// * at the first iteration, the message length is pushed down to the size of my buffer (offest=0)
	// * for all the other iterations, when we are running out of space the lenght of the data chunk the user 
	//   wants to write in the driver is downsized to the free space left [buffer_size - buffer_used(offset)]
	if (*off + len > MSG_SIZE) { len = MSG_SIZE - *off; }
	
	// extract a chunk of data (of LEN bytes) from the user buffer and store it in my buffer at position OFFSET
	if ( copy_from_user(&msg + *off, buf, len) != 0 ) { return -EFAULT; }

	// increment offset, the placeholder inside my buffer
	*off += len;

	// return the number of bytes I was able to write in my driver buffer
	// a new iteration of this function is expected if there is still user data pending to be store in the driver
	return len;
}

/*
 *	File operations and function callbacks
 */
static struct file_operations echobox_fops =
{
	.owner   = THIS_MODULE,
	.open    = echobox_open,
	.release = echobox_close,
	.read    = echobox_read,
	.write   = echobox_write
};



/*
 *	CONSTRUCTOR
 */
static int __init echobox_init(void)
{
	if (alloc_chrdev_region(&devnum, 0, 1, "echobox") < 0)
	{
		return -1;
	}
	if ((cl = class_create(THIS_MODULE, "chardrv")) == NULL)
	{
		unregister_chrdev_region(devnum, 1);
		return -1;
	}
	if (device_create(cl, NULL, devnum, NULL, "echobox") == NULL)
	{
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}

	cdev_init(&mydev, &echobox_fops);
	if (cdev_add(&mydev, devnum, 1) == -1)
	{
		device_destroy(cl, devnum);
		class_destroy(cl);
		unregister_chrdev_region(devnum, 1);
		return -1;
	}

	printk(KERN_INFO "Echobox registered, <Major, Minor>: <%d, %d>\n", MAJOR(devnum), MINOR(devnum));
	return 0;
}

/*
 *	DECONSTRUCTOR
 */
static void __exit echobox_exit(void)
{
	cdev_del(&mydev);
	device_destroy(cl, devnum);
	class_destroy(cl);
	unregister_chrdev_region(devnum, 1);
	printk(KERN_INFO "Echobox unregistered");
}

module_init(echobox_init);
module_exit(echobox_exit);
 
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EchoBox test module, Oct. 2013");
MODULE_AUTHOR("Massimo Testa");
