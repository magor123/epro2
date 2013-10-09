#include <linux/module.h> // Needed by all modules */
#include <linux/kernel.h> // Needed for KERN_ALERT */
#include <linux/init.h>   // included for __init and __exit macros

static int __init hello_start(void)
{
    printk(KERN_INFO "Loading hello module...\n");
    printk(KERN_INFO "Hello world\n");
    return 0;
}

static void __exit hello_end(void)
{
    printk(KERN_INFO "Goodbye Mr.\n");
}

module_init(hello_start);
module_exit(hello_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Massimo");
MODULE_DESCRIPTION("Hello world test module, Oct. 2013");
