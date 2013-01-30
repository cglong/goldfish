#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_ALERT */

MODULE_LICENSE("GPL");
MODULE_AUTHOR("emos, 2013");
MODULE_DESCRIPTION("Demo module");

int init_module(void)
{
    printk(KERN_DEBUG "Hello world.\n"); // Debug level : 7
    return 0;
}

void cleanup_module(void)
{
    printk(KERN_ALERT "Goodbye world.\n"); // Alert level : 1
}
