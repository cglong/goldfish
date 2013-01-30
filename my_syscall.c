#include <linux/jiffies.h>

SYSCALL_DEFINE0(getjiffies)
{
    return (long)get_jiffies_64();
}
