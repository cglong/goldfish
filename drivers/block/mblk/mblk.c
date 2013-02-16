#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");

struct mblk_stat
{
	unsigned total_read_ops;
	unsigned total_write_ops;
	unsigned total_read_size;  // in bytes
	unsigned total_write_size; // in bytes
};

struct mblk_dev
{
    int size;
    u8 *data;
    struct mblk_stat stat;
    spinlock_t lock;
    struct request_queue *queue;
    struct gendisk *gd;
};

#define NSECTORS 1024
#define KERNEL_SECTOR_SIZE 512
#define IOCTL_GET_STATISTICS 48

static struct mblk_dev *Devices = NULL;
static int major_number = 0;
static int device_num = 1;
module_param(device_num, int, 0);

static void mblk_transfer(struct mblk_dev *dev, unsigned long sector, unsigned long nsect, char *buffer, int write)
{
	unsigned long offset = sector * KERNEL_SECTOR_SIZE;
	unsigned long nbytes = nsect * KERNEL_SECTOR_SIZE;
	
	if ((offset + nbytes) > dev->size)
	{
		printk(KERN_NOTICE "mblk: Beyond-end write (%ld %ld)\n", offset, nbytes);
		return;
	}
	
	if (write)
	{
		memcpy(dev->data + offset, buffer, nbytes);
		dev->stat.total_write_ops += 1;
		dev->stat.total_write_size += nbytes;
	}
	else
	{
		memcpy(buffer, dev->data + offset, nbytes);
		dev->stat.total_read_ops += 1;
		dev->stat.total_read_size += nbytes;
	}
}

static void mblk_request(struct request_queue *q)
{
	struct request *req;
	
	while ((req = elv_next_request(q)) != NULL)
	{
		struct mblk_dev *dev = req->rq_disk->private_data;
		if (!blk_fs_request(req))
		{
			printk(KERN_NOTICE "mblk: Skip non-fs request\n");
			end_request(req, 0);
			continue;
		}
		mblk_transfer(dev, req->sector, req->current_nr_sectors, req->buffer, rq_data_dir(req));
		end_request(req, 1);
	}
}

int mblk_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct mblk_dev *dev = filp->private_data;
	struct mblk_stat stat = dev->stat;
	
	switch (cmd)
	{
		case IOCTL_GET_STATISTICS:
			if (copy_to_user((void __user *)arg, &stat, sizeof(stat)))
			{
				return -EFAULT;
			}
			return 0;
	}
	return -ENOTTY;
}

static struct block_device_operations mblk_ops =
{
	.owner = THIS_MODULE,
	.ioctl = mblk_ioctl,
};

static void setup_device(struct mblk_dev *dev, int which)
{
    memset(dev, 0, sizeof(struct mblk_dev));
    dev->size = NSECTORS * KERNEL_SECTOR_SIZE;
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
	set_capacity(dev->gd, NSECTORS);
	add_disk(dev->gd);
}

static int __init mblk_init(void)
{
	int i;
	
    major_number = register_blkdev(major_number, "mblk");
    if (major_number <= 0)
    {
        printk(KERN_WARNING "mblk: Unable to get major number\n");
        return -EBUSY;
    }
    
    Devices = kmalloc(device_num * sizeof(struct mblk_dev), GFP_KERNEL);
    if (Devices == NULL)
    {
		unregister_blkdev(major_number, "mblk");
		return -ENOMEM;
	}
	
	for (i = 0; i < device_num; i++)
	{
		setup_device(Devices + i, i);
	}
    
    printk(KERN_INFO "mblk: Driver loaded.\n");
    return 0;
}

static void __exit mblk_exit(void)
{
	int i;
	for (i = 0; i < device_num; i++)
	{
		struct mblk_dev *dev = Devices + i;
		
		if (dev->gd)
		{
			del_gendisk(dev->gd);
			put_disk(dev->gd);
		}
		
		if (dev->queue)
		{
			blk_cleanup_queue(dev->queue);
		}
		
		if (dev->data)
		{
			vfree(dev->data);
		}
	}
	kfree(Devices);
	
    unregister_blkdev(major_number, "mblk");
    printk(KERN_INFO "mblk: Driver unloaded.\n");
}

module_init(mblk_init);
module_exit(mblk_exit);
