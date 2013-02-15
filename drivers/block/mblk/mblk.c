#include <linux/module.h>

MODULE_LICENSE("GPL");

static int device_num = 1;
module_param(device_num, int, 0);

static int __init mblk_init(void)
{
    printk(KERN_INFO "mblk: Driver loaded.\n");
    return 0;
}

static void __exit mblk_exit(void)
{
    printk(KERN_INFO "mblk: Driver unloaded.\n");
}

module_init(mblk_init);
module_exit(mblk_exit);
