#ifndef WINTERFS_SB
#define WINTERFS_SB

#include <linux/types.h>
#include <linux/fs.h>
#include "winterfs.h"

#define WINTERFS_MAGIC		0x574e4653

#define WINTERFS_SUPERBLOCK_LBA		0
#define WINTERFS_INODE_LIST_HEAD_LBA	1
#define WINTERFS_FREE_LIST_HEAD_LBA	2

// on-disk structures
struct winterfs_indirect_block_list {
	__le64 blocks[WINTERFS_BLOCK_SIZE / sizeof(__le64)];
} __attribute__((packed));

struct winterfs_block_list {
	__le64 size; // number of items in the list
	__le64 num_nodes; // number of blocks the list occupies
	__le64 list_head; // block index of the first node
	__le64 list_tail; // block index of the last node
} __attribute__((packed));

struct winterfs_free_list_node {
	__le64 list[(WINTERFS_BLOCK_SIZE / sizeof(__le64)) - 1];
	__le64 next;
} __attribute__((packed));

struct winterfs_inode_list_node {
	struct winterfs_inode list[(WINTERFS_BLOCK_SIZE / WINTERFS_INODE_SIZE) - 1];
	u8 pad[WINTERFS_INODE_SIZE - sizeof(__le64)];
	__le64 next;
} __attribute__((packed));

struct winterfs_superblock {
	__le32 magic;
        __le64 num_inodes;
        __le64 num_blocks;

	struct winterfs_block_list free_block_list;
	struct winterfs_block_list inode_list;

        u8 block_size; // actual size = 2^block_size
} __attribute__((packed));

// in-memory structures
struct winterfs_sb_info {
	u64 num_inodes;
	u64 num_blocks;
	struct super_block *vfs_sb;
	spinlock_t s_lock;
};

#endif // WINTERFS_SB
