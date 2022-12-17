#include <linux/init.h>
#include <linux/module.h>

static int __init init_winterfs_fs(void)
{
	printk(KERN_ALERT "Hello world");
	return 0;
}

static void __exit exit_winterfs_fs(void)
{

}

module_init(init_winterfs_fs)
module_exit(exit_winterfs_fs)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("winterfs");
MODULE_AUTHOR("nfrizzell");
