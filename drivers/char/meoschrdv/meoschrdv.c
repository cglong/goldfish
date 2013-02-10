#include <linux/module.h>

MODULE_LICENSE("GPL");

#define MEOSCHRDV_DEVICE_NAME "meoschrdv"

static int debug_enable = 0;
module_param(debug_enable, int, 0);


static int __init meoschrdv_init(void)
{
  printk(KERN_INFO "Moes Char Device Driver started.\n");
  printk(KERN_INFO "Currently debug mode is %s.\n", (debug_enable ? "enabled":"disabled"));
  return 0;
}

static void __exit meoschrdv_exit(void)
{
  printk(KERN_INFO "Moes Char Device Driver Removed.\n");
  return;
}

module_init(meoschrdv_init);
module_exit(meoschrdv_exit);
