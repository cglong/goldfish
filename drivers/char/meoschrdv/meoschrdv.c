#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "meoschrdv.h"

MODULE_LICENSE("GPL");


static int debug_enable = 0;
module_param(debug_enable, int, 0);

struct file_operations meoschrdv_fops;
static struct meoschrdv my_dev;
static struct class *my_dev_class = NULL;

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

static ssize_t meoschrdv_read(struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
  printk(KERN_INFO "Read %d bytes from %p.\n", count, buf);
  return 0;
}

static ssize_t meoschrdv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
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
  int err = 0;
  dev_t dev_no = 0;
  struct device *device = NULL;
  
  printk(KERN_INFO "Moes Char Device Driver started.\n");
  printk(KERN_INFO "Currently debug mode is %s.\n", (debug_enable ? "enabled":"disabled"));
  printk(KERN_INFO "Hello\n");

  // Get device numbers
  err = alloc_chrdev_region(&dev_no, MEOSCHRDV_MINOR_BASE, MEOSCHRDV_NUM_DEVICES, MEOSCHRDV_DEVICE_NAME);
  if (err < 0)
    {
      printk(KERN_ERR "Device number allocation failed.\n");
      return err;
    }

  // Create device class
  my_dev_class = class_create(THIS_MODULE, MEOSCHRDV_DEVICE_NAME);
  if (IS_ERR(my_dev_class))
    return PTR_ERR(my_dev_class);

  cdev_init(&my_dev.cdev, &meoschrdv_fops);
  my_dev.cdev.owner = THIS_MODULE;
  my_dev.cdev.ops = &meoschrdv_fops;
  err = cdev_add(&my_dev.cdev, dev_no, MEOSCHRDV_NUM_DEVICES);

  if (err != 0)
    {
      printk(KERN_ERR "Error %d while adding %s.\n", err, MEOSCHRDV_DEVICE_NAME);
    }
  
  device = device_create(my_dev_class, NULL,
			 dev_no, NULL,
			 MEOSCHRDV_DEVICE_NAME);

  if (IS_ERR(device)) {
    err = PTR_ERR(device);
    printk(KERN_WARNING "Error %d while trying to create %s",
	   err, MEOSCHRDV_DEVICE_NAME);
    cdev_del(&my_dev.cdev);
    return err;
  }

  printk(KERN_INFO "%s is successfully loaded.\n",MEOSCHRDV_DEVICE_NAME);

  return 0;
  
}

static void __exit meoschrdv_exit(void)
{
  cdev_del(&my_dev.cdev);

  printk(KERN_INFO "Moes Char Device Driver Removed.\n");
  return;
}

module_init(meoschrdv_init);
module_exit(meoschrdv_exit);
