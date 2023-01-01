#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_sb.h"
#include "winterfs_ino.h"

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
};

const struct file_operations winterfs_dir_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir
};
