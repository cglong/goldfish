#include <linux/fs.h>
#include <linux/module.h>
#include <linux/vmalloc.h>

MODULE_LICENSE("GPL");

struct mblk_dev
{
    int size;
    u8 *data;
    spinlock_t lock;
};

static int major_number = 0;
static int device_num = 1;
module_param(device_num, int, 0);

static void setup_device(struct mblk_dev *dev, int which)
{
    memset(dev, 0, sizeof(struct mblk_dev));
    dev->size = 1024 * 512;
    dev->data = vmalloc(dev->size);
    if (dev->data == NULL)
    {
        printk(KERN_NOTICE "mblk: Vmalloc failure\n");
        return;
    }
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
