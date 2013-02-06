#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simplified scheduling info kernel module");
MODULE_AUTHOR("Chris Long");

static struct proc_dir_entry *proc_entry;
long pid;

int myschedinfo_read(char *page, char **start, off_t off, int count, int *eof, void *data);
ssize_t myschedinfo_write(struct file *filp, const char __user *buff, unsigned long len, void *data);

int init_myschedinfo_module(void)
{
    int ret = 0;
    
    proc_entry = create_proc_entry("myschedinfo", 0644, NULL);
    if (proc_entry != NULL)
    {
        proc_entry->read_proc = myschedinfo_read;
        proc_entry->write_proc = myschedinfo_write;
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

int myschedinfo_read(char *page, char **start, off_t off, int count, int *eof, void *data)
{
    struct sched_entity se;
    int len;
    
    se = find_task_by_vpid(pid)->se;
    
    len = sprintf(page, "exec_start: %lld\n", se.exec_start);
    len += sprintf(page+len, "sum_exec_runtime: %lld\n", se.sum_exec_runtime);
    len += sprintf(page+len, "vruntime: %lld\n", se.vruntime);
    len += sprintf(page+len, "prev_sum_exec_runtime: %lld\n", se.prev_sum_exec_runtime);
    len += sprintf(page+len, "last_wakeup: %lld\n", se.last_wakeup);
    
    return len;
}

ssize_t myschedinfo_write(struct file *filp, const char __user *buff, unsigned long len, void *data)
{
    pid = simple_strtol(buff, NULL, 10);
    return len;
}

module_init(init_myschedinfo_module);
module_exit(cleanup_myschedinfo_module);
