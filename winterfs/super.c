#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

static void winterfs_put_super(struct super_block *sb)
{
	struct winterfs_sb_info *sbi;

	sbi = sb->s_fs_info;
	brelse(sbi->sb_buf);
	kfree(sbi);
}

const static struct super_operations winterfs_super_operations = {
	//.put_super = winterfs_put_super,
	.statfs = simple_statfs
};

static int winterfs_fill_super(struct super_block *sb, void *data, int silent)
{
	int ret;
	struct buffer_head *sb_buf;
	struct winterfs_superblock *ws;
	struct winterfs_sb_info *sbi;
	struct inode *root;

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

	sbi->sb_buf = sb_buf;
	ws = (struct winterfs_superblock *)sb_buf->b_data;
	sbi->num_inodes = le32_to_cpu(ws->num_inodes);
	sbi->num_blocks = le32_to_cpu(ws->num_blocks);
	sbi->free_inode_bitset_idx = le32_to_cpu(ws->free_inode_bitset_idx);
	sbi->free_block_bitset_idx = le32_to_cpu(ws->free_block_bitset_idx);
	sbi->bad_block_bitset_idx = le32_to_cpu(ws->bad_block_bitset_idx);
	sbi->data_blocks_idx = le32_to_cpu(ws->data_blocks_idx);

	sb->s_magic 		= be32_to_cpu(ws->magic);
	sb->s_maxbytes 		= WINTERFS_MAX_FILE_SIZE;
	sb->s_blocksize 	= WINTERFS_BLOCK_SIZE;
	sb->s_blocksize_bits 	= 12;
	sb->s_op		= &winterfs_super_operations;
	sb->s_time_gran		= WINTERFS_TIME_RES; // 1 sec
	strcpy(sb->s_id, "winterfs");

	if (sb->s_magic != WINTERFS_MAGIC) {
		ret = -EINVAL;
                goto err;
	}

	root = winterfs_iget(sb, WINTERFS_ROOT_INODE);
        if (IS_ERR(root)) {
                ret = PTR_ERR(root);
                goto err;
        }

	inode_init_owner(&init_user_ns, root, NULL, S_IFDIR | 0755);

	sb->s_root = d_make_root(root);
        if (!sb->s_root) {
                printk(KERN_ERR "Get root inode failed");
                ret = -ENOMEM;
                goto err;
        }

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

	BUILD_BUG_ON(sizeof(struct winterfs_superblock) > WINTERFS_BLOCK_SIZE);
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
