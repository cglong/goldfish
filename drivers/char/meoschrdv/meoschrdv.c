#include <linux/module.h>
#include <linux/fs.h>


MODULE_LICENSE("GPL");

#define MEOSCHRDV_DEVICE_NAME "meoschrdv"

static int debug_enable = 0;
module_param(debug_enable, int, 0);

struct file_operations meoschrdv_fops;

static int meoschrdv_open(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "Opened meoschrdv successfully.\n");
  return 0;
}

static int meoschrdv_release(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "Released meoschrdv successfully.\n");
  return 0;
}

static ssize_t meoschrdv_read(struct file *file, char *buf, size_t count, loff_t *ptr)
{
  printk(KERN_INFO "Read %d bytes from %p.\n", count, buf);
  return 0;
}

static ssize_t meoschrdv_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
  printk(KERN_INFO "Write %d bytes to %p.\n", count, buf);
  return 0;
}

struct file_operations meoschrdv_fops = {
  .owner = THIS_MODULE,
  .read = meoschrdv_read,
  .write = meoschrdv_write,
  .open = meoschrdv_open,
  .release = meoschrdv_release,
};


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
