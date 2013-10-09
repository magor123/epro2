/*
	Echobox, a Simple Linux Character Device Driver
	
	Echobox device can store a string in an internal buffer and then print it back.
	The device Major number is assigned statically.

	This simple charachter driver is meant for demonstration purposes only.
	The use of this code is DEPRECATED due to use of an outdated registration interface.

	Try this at home if you want.
*/

#include <linux/module.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("EchoBox test module, Oct. 2013");
MODULE_AUTHOR("Massimo Testa");

static int   MAJOR_NUMBER = 32;		// my Major number
static char  msg[100] = {0};		// Device driver internal memory
static short pos = 0;			// placeholder for the internal memory

/*
 *	Prototypes
 */
static int     echobox_open    ( struct inode * , struct file *);
static int     echobox_release ( struct inode * , struct file *);
static ssize_t echobox_read    ( struct file * , char *,       size_t , loff_t *);
static ssize_t echobox_write   ( struct file * , const char *, size_t , loff_t *);


/*
 *	Define the FILE OPERATIONS and associated functions
 */
static struct file_operations fops = {
	.read    = echobox_read,
	.write   = echobox_write,
	.open    = echobox_open,
	.release = echobox_release
};


/*
 *	Called when module is loaded
 */
int init_module(void)
{
	int t = register_chrdev(MAJOR_NUMBER, "echobox", &fops);

	if(t<0) { printk(KERN_ALERT "Echobox registration Failed !\n"); }
	else { printk(KERN_ALERT "Echobox successfully registered !\n"); }

	return t;
}


/*
 *	Called when module is unloaded
 */
void cleanup_module(void)
{
	printk(KERN_ALERT "Echobox unregistered\n");
	unregister_chrdev(MAJOR_NUMBER, "echobox");
}


/*
 *	Called when you OPEN the device file /dev/echobox
 */
static int echobox_open(struct inode *inodep, struct file *filep)
{
	printk(KERN_ALERT "Echobox opened\n");
	return 0;
}


/*
 *	Called when you WRITE something to the device file /dev/echobox
 */
static ssize_t echobox_write(struct file *filep, const char *buff, size_t len, loff_t *off)
{
	short count = 0;
	memset(msg,0,100);
	pos = 0;
	while(len >0) {
		msg[count] = buff[count];
		count++;
		len--;
	}
	return count;
}


/*
 *	Called when you READ the device file /dev/echobox
 */
static ssize_t echobox_read(struct file *filep, char *buff, size_t len, loff_t *off)
{
	short count = 0;
	while(len && (msg[pos]!=0))
	{
		put_user(msg[pos],buff++); // copy byte from kernel space to user space 
		count++;
		len--;
		pos++;
	}
	return count;
}


/*
 *	Called when you CLOSE the device file /dev/echobox
 */
static int echobox_release(struct inode *inodep, struct file *filep)
{
	printk(KERN_ALERT "Echobox closed\n");
	return 0;
}

