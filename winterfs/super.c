#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

static int winterfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret;
	struct winterfs_superblock *ws;
	struct winterfs_sb_info *sbi;
	struct buffer_head *sb_buf;

	sbi = kzalloc(sizeof(struct winterfs_sb_info), GFP_KERNEL);
	if (!sbi) {
		return ENOMEM;
	}

	spin_lock_init(&(sbi->s_lock));
	sbi->vfs_sb = sb;
	sb->s_fs_info = sbi;

	sb_buf = sb_bread(sb, WINTERFS_SUPERBLOCK_LBA);
	if (!sb_buf) {
		printk(KERN_ERR "Error reading superblock from disk");
		goto err;
	}

	ws = (struct winterfs_superblock *)sb_buf->b_data;
	sb->s_magic = le32_to_cpu(ws->magic);
	sb->s_blocksize = (1 << ws->block_size);

	return 0;
err:
	sb->s_fs_info = NULL;
	kfree(sbi);
	return ret;	
}

static struct dentry *winterfs_mount(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data)
{
	return mount_bdev(fs_type, flags, dev_name, data, winterfs_fill_super);
}

const struct super_operations winterfs_super_operations = {

};

static struct file_system_type winterfs_fs_type = {
	.owner			= THIS_MODULE,
	.name			= "winterfs",
	.mount			= winterfs_mount,
	.kill_sb		= kill_block_super,
	.fs_flags		= FS_REQUIRES_DEV
};

static int __init init_winterfs_fs(void)
{
	int err;

	BUILD_BUG_ON(sizeof(struct winterfs_free_list_node) != WINTERFS_BLOCK_SIZE);
	BUILD_BUG_ON(sizeof(struct winterfs_inode_list_node) != WINTERFS_BLOCK_SIZE);
	BUILD_BUG_ON(sizeof(struct winterfs_superblock) > WINTERFS_BLOCK_SIZE);

	BUILD_BUG_ON(sizeof(struct winterfs_inode) != WINTERFS_INODE_SIZE);
	BUILD_BUG_ON(sizeof(struct winterfs_indirect_block_list) != WINTERFS_BLOCK_SIZE);

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
