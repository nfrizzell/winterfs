#ifndef WINTERFS_SB
#define WINTERFS_SB

#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/fs.h>
#include "winterfs.h"

#define WINTERFS_MAGIC	 	0x574e4653

#define WINTERFS_SUPERBLOCK_BLOCK_IDX	0
#define WINTERFS_INODES_BLOCK_IDX	1

#define WINTERFS_ROOT_INODE		1

// on-disk structure
struct winterfs_superblock {
	__le32 magic;
        __le32 num_inodes;
        __le32 num_blocks;
	__le32 free_inode_bitset_idx;
	__le32 free_block_bitset_idx;
	__le32 bad_block_bitset_idx;
	__le32 data_blocks_idx;
} __attribute__((packed));

// in-memory structure
struct winterfs_sb_info {
	u32 num_inodes;
	u32 num_blocks;
	u32 free_inode_bitset_idx;
	u32 free_block_bitset_idx;
	u32 bad_block_bitset_idx;
	u32 data_blocks_idx;

	struct super_block *vfs_sb;
	struct buffer_head *sb_buf;
	spinlock_t s_lock;
};

#endif // WINTERFS_SB
