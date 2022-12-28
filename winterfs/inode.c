#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

struct inode *winterfs_iget (struct super_block *sb, u64 ino)
{
	struct winterfs_superblock *wsb = (struct winterfs_superblock *) sb->s_fs_info;
	u64 inode_block = (ino * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE;
	u64 inode_block_lba = inode_block + wsb->first_inode_idx;
	u16 inode_offset = (ino * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE;

	struct buffer_head *bh = sb_bread(sb, inode_block_lba);

	return NULL;
}
