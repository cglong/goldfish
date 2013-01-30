#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simplified memory info kernel module");
MODULE_AUTHOR("Chris Long");

static struct proc_dir_entry *proc_entry;

int init_mymeminfo_module()
{
    return -1;
}

void cleanup_mymeminfo_module()
{
}

module_init(init_mymeminfo_module);
module_exit(cleanup_mymeminfo_module);
