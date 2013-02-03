#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simplified scheduling info kernel module");
MODULE_AUTHOR("Chris Long");

static struct proc_dir_entry *proc_entry;

int init_myschedinfo_module(void)
{
    int ret = 0;
    
    proc_entry = create_proc_entry("myschedinfo", 0644, NULL);
    if (proc_entry != NULL)
    {
        proc_entry->owner = THIS_MODULE;
        printk(KERN_INFO "myschedinfo: Module loaded.\n");
    }
    else
    {
        ret = -ENOMEM;
        printk(KERN_INFO "myschedinfo: Couldn't create proc entry\n");
    }
    return ret;
}

void cleanup_myschedinfo_module(void)
{
    remove_proc_entry("myschedinfo", proc_entry);
    printk(KERN_INFO "myschedinfo: Module unloaded.\n");
}

module_init(init_myschedinfo_module);
module_exit(cleanup_myschedinfo_module);
