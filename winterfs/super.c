#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include "winterfs.h"

static int winterfs_fill_super(struct super_block *sb, void *data, int silent)
{
	return 0;
}

static struct dentry *winterfs_mount(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data)
{
	printk(KERN_ALERT "Mounting winterfs");
	return mount_bdev(fs_type, flags, dev_name, data, winterfs_fill_super);
}

const struct super_operations winterfs_super_operations = {

};

static struct file_system_type winterfs_fs_type = {
	.mount	= winterfs_mount,
	.name	= "winterfs",
	.owner	= THIS_MODULE,
};

static int __init init_winterfs_fs(void)
{
	int err;

	BUILD_BUG_ON(sizeof(struct winterfs_superblock) != WINTERFS_SB_SIZE);
	BUILD_BUG_ON(sizeof(struct winterfs_indirect_block_list) != WINTERFS_BLOCK_SIZE);
	BUILD_BUG_ON(sizeof(struct winterfs_inode) != WINTERFS_INODE_SIZE);

	err = register_filesystem(&winterfs_fs_type);
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
