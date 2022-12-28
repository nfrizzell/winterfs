#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

struct inode *winterfs_iget (struct super_block *sb, u64 ino)
{
	
	return NULL;
}

struct winterfs_inode *winterfs_get_inode(struct super_block *sb, 
	ino_t ino, struct buffer_head **p)
{
	struct winterfs_sb_info *sbi;
	struct buffer_head *bh;
	struct winterfs_inode *inode;
	u64 inode_block; 	
	u64 inode_block_lba;
	u16 offset; 

	sbi = (struct winterfs_sb_info *) sb->s_fs_info;
	inode_block = (ino * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE;
	inode_block_lba = inode_block + sbi->first_inode_idx;
	offset = (ino * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE;

	bh = sb_bread(sb, inode_block_lba);
	if (!bh) {
		printk(KERN_ERR "Error reading block %llu\n", inode_block_lba);
		return ERR_PTR(-EIO);
	}

	inode = kzalloc(sizeof(struct winterfs_inode), GFP_KERNEL);
	if (!inode) {
		return ERR_PTR(-ENOMEM);
	}

	memcpy(bh->b_data + offset, inode, sizeof(struct winterfs_inode));
	return inode;
}
