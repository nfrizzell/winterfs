#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_dir.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

static struct winterfs_dir_block_info *winterfs_dir_load_block(struct super_block *sb, u32 dir_block_idx)
{
	struct buffer_head *bh;
	struct winterfs_dir_block_info *wdbi;
	int err;

	wdbi = kzalloc(sizeof(struct winterfs_dir_block_info), GFP_KERNEL);
	if (!wdbi) {
		return ERR_PTR(-ENOMEM);
	}

	bh = sb_bread(sb, dir_block_idx);
	if (!bh) {
		printk(KERN_ERR "Error reading block %u\n", dir_block_idx);
		err = -EIO;
		goto cleanup;
	}

	wdbi->bh = bh;
	wdbi->db = (struct winterfs_dir_block *)bh->b_data;

	return wdbi;
cleanup:
	kfree(wdbi);
	return ERR_PTR(err);
}

static struct dentry *winterfs_lookup(struct inode *dir, struct dentry *dentry, 
	unsigned int flags)
{
	struct winterfs_dir_block_info *wdbi;
	struct super_block *sb;

	if (dentry->d_name.len > WINTERFS_FILENAME_MAX_LEN) {
		return ERR_PTR(-ENAMETOOLONG);
	}

	sb = dir->i_sb;

	wdbi = winterfs_dir_load_block(sb, 0);

	printk(KERN_ERR "dir name: %s\n", dentry->d_name.name);
	return NULL;
}

static int winterfs_create (struct user_namespace *mnt_userns, struct inode *dir,
	struct dentry *dentry, umode_t mode, bool excl)
{
	return 0;
}

static int winterfs_mkdir(struct user_namespace *mnt_userns,
        struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;

	inode = winterfs_new_inode(dir, S_IFDIR | mode, &(dentry->d_name));

	inode->i_op = &winterfs_dir_inode_operations;
        inode->i_fop = &winterfs_dir_operations;

	d_instantiate_new(dentry, inode);

	return 0;
}

const struct inode_operations winterfs_dir_inode_operations = {
	.mkdir		= winterfs_mkdir,
	.create		= winterfs_create,
	.lookup		= winterfs_lookup
};

const struct file_operations winterfs_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir
};
