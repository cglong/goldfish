#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>

#include "mchr.h"

MODULE_LICENSE("GPL");


static int debug_enable = 0;
module_param(debug_enable, int, 0);

struct file_operations mchr_fops;
static struct mchr mydev;
static struct class *pmyclass = NULL;
static unsigned int devno_major = 0;
static int mchr_open(struct inode *inode, struct file *pfile)
{
  if (devno_major != imajor(inode))
    {
      printk(KERN_WARNING "No device found with major=%d\n", devno_major);
      return -ENODEV;
    }

  pfile->private_data = &mydev;

  if (mydev.data == NULL)
    {
      mydev.data = (unsigned char*)kzalloc(mydev.bufsize, GFP_KERNEL);
      if (mydev.data == NULL)
	{
	  printk(KERN_WARNING "open(): out of memory\n");
	  return -ENOMEM;
	}
    }

  printk(KERN_INFO "Opened mchr successfully.\n");

  return 0;
}

static int mchr_release(struct inode *inode, struct file *pfile)
{
  printk(KERN_INFO "Released mchr successfully.\n");
  return 0;
}

static ssize_t mchr_read(struct file *pfile, char __user *buf, size_t count, loff_t *ppos)
{
  struct mchr *pdev = (struct mchr *)pfile->private_data;
  ssize_t retval = 0;
  int i = 0;

  if (*ppos >= pdev->bufsize)
    {
      return retval;
    }

  if (*ppos + count > pdev->bufsize)
    {
      count = pdev->bufsize - *ppos;
    }

  if (copy_to_user(buf, &(pdev->data[*ppos]), count) != 0)
    {
      return -EFAULT;
    }

  printk(KERN_INFO "*ppos: %lld\n", *ppos);
  for (i = 0; i < count; i++)
    {
      //      printk(KERN_INFO "%c", pdev->data[*ppos + i]);
    }
  
  *ppos += count;
  retval = count;

  printk(KERN_INFO "\nRead %d bytes from %p.\n", count, buf);
  return retval;
}

static ssize_t mchr_write(struct file *pfile, const char __user *buf, size_t count, loff_t *ppos)
{
  struct mchr *pdev = (struct mchr *)pfile->private_data;
  ssize_t retval = 0;
  int i = 0;

  if (*ppos >= pdev->bufsize)
    {
      return -EINVAL;
      
    }

  if (*ppos + count > pdev->bufsize)
    {
      count = pdev->bufsize - *ppos;
    }
  
  if (copy_from_user(&(pdev->data[*ppos]), buf, count) != 0)
  {
    return -EFAULT;
  }

  printk(KERN_INFO "*ppos: %lld\n", *ppos);
  for (i = 0; i < count; i++)
    {
      //      printk(KERN_INFO "%c", pdev->data[*ppos + i]);
    }
  
  *ppos += count;
  retval = count;

  printk(KERN_INFO "\nWrite %d bytes to %p.\n", count, buf);

  return retval;
}

loff_t
mchr_llseek(struct file *pfile, loff_t off, int whence)
{
  struct mchr *pdev = (struct mchr *)pfile->private_data;
  loff_t newpos = 0;

  printk(KERN_INFO "\nllseek off: %lld\n", off);
  switch(whence) {
  case 0: /* SEEK_SET */
    newpos = off;
    break;
 
  case 1: /* SEEK_CUR */
    newpos = pfile->f_pos + off;
    break;
 
  case 2: /* SEEK_END */
    newpos = pdev->bufsize + off;
    break;
 
  default: /* can't happen */
    return -EINVAL;
  }
  if (newpos < 0 || newpos > pdev->bufsize) 
    return -EINVAL;
     
  pfile->f_pos = newpos;
  return newpos;

}  

struct file_operations mchr_fops = {
  .owner = THIS_MODULE,
  .read = mchr_read,
  .write = mchr_write,
  .open = mchr_open,
  .llseek = mchr_llseek,
  .release = mchr_release,
};


static int mchr_setup_device(struct mchr *pmydev, struct class *pclass, dev_t devno)
{
  int err = 0;
  struct device *device = NULL;

  pmydev->data = NULL;
  pmydev->bufsize = MCHR_BUFSIZE;
  

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

static void mchr_cleanup(void)
{
  dev_t dev_no = MKDEV(devno_major, MCHR_MINOR_BASE);
  device_destroy(pmyclass, dev_no);
  cdev_del(&mydev.cdev);

  if (pmyclass)
    {
      class_destroy(pmyclass);
    }
  kfree(mydev.data);

  unregister_chrdev_region(dev_no, 1);
  printk(KERN_INFO "Moes Char Device Driver Removed.\n");
  return;
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
  err = mchr_setup_device(&mydev, pmyclass, dev_no);

  if (err != 0)
    {
      printk(KERN_ERR "Device setup failed.\n");
      mchr_cleanup();
      return err;
    }
  return 0;
  
}

static void __exit mchr_exit(void)
{
  mchr_cleanup();
  return;
}

module_init(mchr_init);
module_exit(mchr_exit);
