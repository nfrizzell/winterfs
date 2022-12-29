#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

struct inode *winterfs_iget (struct super_block *sb, u64 ino)
{
	struct winterfs_inode *wfs_inode;
	struct winterfs_inode_info *wfs_info;
	struct inode *inode;
	struct buffer_head *bh;

	inode = iget_locked(sb, ino);
	if (!inode) {
		return ERR_PTR(-ENOMEM);
	}
	if (!(inode->i_state & I_NEW)) {
                return inode;
	}

	wfs_info = (struct winterfs_inode_info *)inode->i_private;

	wfs_inode = winterfs_get_inode(sb, ino, &bh);
	if (IS_ERR(wfs_inode)) {
		brelse(bh);
		return ERR_PTR(PTR_ERR(wfs_inode));
	}
	if (S_ISREG(inode->i_mode)) {
                inode->i_op = &winterfs_file_inode_operations;
                inode->i_fop = &winterfs_file_operations;
	} else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &simple_dir_inode_operations;
                inode->i_fop = &simple_dir_operations;
	}

	brelse(bh);

	return inode;
}

struct winterfs_inode *winterfs_get_inode(struct super_block *sb, 
	ino_t ino, struct buffer_head **bh_out)
{
	struct buffer_head *bh;
	struct winterfs_inode *wf_inode;
	u32 inode_block; 	
	u32 inode_block_lba;
	u16 offset; 

	inode_block = (ino * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE;
	inode_block_lba = inode_block + WINTERFS_INODES_LBA;
	offset = (ino-1 * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE;

	bh = sb_bread(sb, inode_block_lba);
	*bh_out = bh;
	if (!bh) {
		printk(KERN_ERR "Error reading block %u\n", inode_block_lba);
		return ERR_PTR(-EIO);
	}

	wf_inode = kzalloc(sizeof(struct winterfs_inode), GFP_KERNEL);
	if (!wf_inode) {
		return ERR_PTR(-ENOMEM);
	}

	memcpy(bh->b_data + offset, wf_inode, sizeof(struct winterfs_inode));

	return wf_inode;
}

const struct inode_operations winterfs_file_inode_operations = {

};

const struct inode_operations winterfs_dir_inode_operations = {

};
