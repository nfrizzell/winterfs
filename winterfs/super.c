#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include "winterfs.h"

const struct super_operations winterfs_super_operations = {

};

static const struct file_system_type winterfs_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "winterfs",
};

static int __init init_winterfs_fs(void)
{
	int err = register_filesystem(&winterfs_fs_type);
	return err;
}

static void __exit exit_winterfs_fs(void)
{
	unregister_filesystem(&winterfs_fs_type);
}

module_init(init_winterfs_fs)
module_exit(exit_winterfs_fs)

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("winterfs");
MODULE_AUTHOR("nfrizzell");
