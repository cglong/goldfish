#include <linux/module.h>

MODULE_LICENSE("GPL");

static int __init meoschrdv_init(void)
{
  printk(KERN_INFO "Moes Char Device Driver started.\n");
  return 0;
}

static void __exit meoschrdv_exit(void)
{
  printk(KERN_INFO "Moes Char Device Driver Removed.\n");
  return;
}

module_init(meoschrdv_init);
module_exit(meoschrdv_exit);
