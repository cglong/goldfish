#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>

#include "meoschrdv.h"

MODULE_LICENSE("GPL");


static int debug_enable = 0;
module_param(debug_enable, int, 0);

struct file_operations mchr_fops;
static struct mchr pmydev;
static struct class *pmyclass = NULL;
static unsigned int devno_major = 0;
static int mchr_open(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "Opened mchr successfully.\n");
  return 0;
}

static int mchr_release(struct inode *inode, struct file *file)
{
  printk(KERN_INFO "Released mchr successfully.\n");
  return 0;
}

static ssize_t mchr_read(struct file *file, char __user *buf, size_t count, loff_t *ptr)
{
  printk(KERN_INFO "Read %d bytes from %p.\n", count, buf);
  return 0;
}

static ssize_t mchr_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
  printk(KERN_INFO "Write %d bytes to %p.\n", count, buf);
  return 0;
}

struct file_operations mchr_fops = {
  .owner = THIS_MODULE,
  .read = mchr_read,
  .write = mchr_write,
  .open = mchr_open,
  .release = mchr_release,
};


static int mchr_setup_device(struct mchr *pmydev, struct class *pclass, dev_t devno)
{
  int err = 0;
  struct device *device = NULL;

  cdev_init(&pmydev->cdev, &mchr_fops);
  pmydev->cdev.owner = THIS_MODULE;
  pmydev->cdev.ops = &mchr_fops;
  err = cdev_add(&pmydev->cdev, devno, MCHR_NUM_DEVICES);

  if (err != 0)
    {
      printk(KERN_ERR "Error %d while adding %s.\n", err, MCHR_DEVICE_NAME);
    }

  
  device = device_create(pclass, NULL,
			 devno, NULL,
			 MCHR_DEVICE_NAME);

  if (IS_ERR(device)) {
    err = PTR_ERR(device);
    printk(KERN_WARNING "Error %d while trying to create %s",
	   err, MCHR_DEVICE_NAME);
    cdev_del(&pmydev->cdev);
    return err;
  }
  return err;
}

static int __init mchr_init(void)
{
  int err = 0;
  dev_t dev_no = 0;
  
  printk(KERN_INFO "Moes Char Device Driver started.\n");
  printk(KERN_INFO "Currently debug mode is %s.\n", (debug_enable ? "enabled":"disabled"));

  // Get device numbers
  err = alloc_chrdev_region(&dev_no, MCHR_MINOR_BASE, MCHR_NUM_DEVICES, MCHR_DEVICE_NAME);
  if (err < 0)
    {
      printk(KERN_ERR "Device number allocation failed.\n");
      return err;
    }

  devno_major = MAJOR(dev_no);

  // Create device class
  pmyclass = class_create(THIS_MODULE, MCHR_DEVICE_NAME);
  if (IS_ERR(pmyclass))
    return PTR_ERR(pmyclass);

  // Set up the device
  err = mchr_setup_device(&pmydev, pmyclass, dev_no);

  if (err != 0)
    {
      printk(KERN_ERR "Device setup failed.\n");
      return err;
    }
  return 0;
  
}

static void __exit mchr_exit(void)
{
  device_destroy(pmyclass, MKDEV(devno_major, MCHR_MINOR_BASE));
  cdev_del(&pmydev.cdev);

  if (pmyclass)
    {
      class_destroy(pmyclass);
    }

  printk(KERN_INFO "Moes Char Device Driver Removed.\n");
  return;
}

module_init(mchr_init);
module_exit(mchr_exit);
