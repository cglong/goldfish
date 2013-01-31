#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simplified memory info kernel module");
MODULE_AUTHOR("Chris Long");

static struct proc_dir_entry *proc_entry;

int mymeminfo_read(char *page, char **start, off_t off, int count, int *eof, void *data);
inline unsigned long kilobytes(unsigned long bytes);

int init_mymeminfo_module(void)
{
    int ret = 0;
    
    proc_entry = create_proc_entry("mymeminfo", 0644, NULL);
    if (proc_entry != NULL)
    {
        proc_entry->read_proc = mymeminfo_read;
        proc_entry->owner = THIS_MODULE;
        printk(KERN_INFO "mymeminfo: Module loaded.\n");
    }
    else
    {
        ret = -ENOMEM;
        printk(KERN_INFO "mymeminfo: Couldn't create proc entry\n");
    }
    return ret;
}

void cleanup_mymeminfo_module(void)
{
    remove_proc_entry("mymeminfo", proc_entry);
    printk(KERN_INFO "mymeminfo: Module unloaded.\n");
}

int mymeminfo_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    struct sysinfo i;
    int len;
    
    si_meminfo(&i);
    
    len = sprintf(page, "Total usable RAM: %lu KB\n", kilobytes(i.totalram));
    len += sprintf(page+len, "Total free RAM: %lu KB\n", kilobytes(i.freeram));
    
    return len;
}

inline unsigned long kilobytes(unsigned long bytes)
{
    return bytes << (PAGE_SHIFT - 10);
}

module_init(init_mymeminfo_module);
module_exit(cleanup_mymeminfo_module);
