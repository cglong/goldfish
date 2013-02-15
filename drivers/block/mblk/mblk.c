#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");

struct mblk_dev
{
    int size;
    u8 *data;
    spinlock_t lock;
    struct request_queue *queue;
    struct gendisk *gd;
};

static struct block_device_operations mblk_ops =
{
	.owner = THIS_MODULE,
};

static int major_number = 0;
static int device_num = 1;
module_param(device_num, int, 0);
static int nsectors = 1024;

static void mblk_request(struct request_queue_t *q)
{
}

static void setup_device(struct mblk_dev *dev, int which)
{
    memset(dev, 0, sizeof(struct mblk_dev));
    dev->size = nsectors * 512;
    dev->data = vmalloc(dev->size);
    if (dev->data == NULL)
    {
        printk(KERN_NOTICE "mblk: Vmalloc failure\n");
        return;
    }
    spin_lock_init(&dev->lock);
    
    dev->queue = blk_init_queue(mblk_request, &dev->lock);
    
    dev->gd = alloc_disk(1);
    if (!dev->gd)
    {
		printk(KERN_NOTICE "mblk: Alloc_disk failure\n");
		if (dev->data)
		{
			vfree(dev->data);
		}
		return;
	}
	
	dev->gd->major = major_number;
	dev->gd->first_minor = which;
	dev->gd->fops = &mblk_ops;
	dev->gd->queue = dev->queue;
	dev->gd->private_data = dev;
	snprintf(dev->gd->disk_name, 32, "mblk%i", which);
	set_capacity(dev->gd, nsectors);
}

static int __init mblk_init(void)
{
    major_number = register_blkdev(major_number, "mblk");
    if (major_number <= 0)
    {
        printk(KERN_WARNING "mblk: Unable to get major number\n");
        return -EBUSY;
    }
    
    printk(KERN_INFO "mblk: Driver loaded.\n");
    return 0;
}

static void __exit mblk_exit(void)
{
    unregister_blkdev(major_number, "mblk");
    printk(KERN_INFO "mblk: Driver unloaded.\n");
}

module_init(mblk_init);
module_exit(mblk_exit);
