#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/swap.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simplified memory info kernel module");
MODULE_AUTHOR("Chris Long");

struct vmalloc_info
{
    unsigned long used;
    unsigned long largest_chunk;
};

static struct proc_dir_entry *proc_entry;

extern void get_vmalloc_info(struct vmalloc_info *vmi);
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
    long cached;
    unsigned long vmalloc_total;
    struct vmalloc_info vmi;
    int len;
    
    si_meminfo(&i);
    cached = global_page_state(NR_FILE_PAGES) - total_swapcache_pages - i.bufferram;
    vmalloc_total = VMALLOC_END - VMALLOC_START;
    get_vmalloc_info(&vmi);
    
    len = sprintf(page, "Total usable RAM size: %lu KB\n", kilobytes(i.totalram));
    len += sprintf(page+len, "Total free memory size: %lu KB\n", kilobytes(i.freeram));
    len += sprintf(page+len, "Cached memory size: %lu KB\n", kilobytes(cached));
    len += sprintf(page+len, "Page table size: %lu KB\n", kilobytes(global_page_state(NR_PAGETABLE)));
    len += sprintf(page+len, "Vmalloc total size: %lu KB\n", vmalloc_total >> 10);
    len += sprintf(page+len, "Vmalloc used size: %lu KB\n", vmi.used >> 10);
    
    return len;
}

inline unsigned long kilobytes(unsigned long bytes)
{
    return bytes << (PAGE_SHIFT - 10);
}

module_init(init_mymeminfo_module);
module_exit(cleanup_mymeminfo_module);
