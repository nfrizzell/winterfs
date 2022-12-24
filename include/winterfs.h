#include <linux/types.h>
#include <linux/fs.h>

#define WINTERFS_BLOCK_SIZE 	4096 // fixed for now...

#define WINTERFS_SB_SIZE	WINTERFS_BLOCK_SIZE

#define WINTERFS_INODE_PRIMARY_BLOCKS 	8
#define WINTERFS_INODE_SIZE 		128
#define WINTERFS_INODE_FILE 		0
#define WINTERFS_INODE_DIR 		1

#define WINTERFS_BYTES_PER_INODE	(15 << 1)
#define WINTERFS_INODES_PER_BLOCK	WINTERFS_BLOCK_SIZE / WINTERFS_INODE_SIZE

typedef __le64 winterfs_block_addr;

struct winterfs_superblock {
        __le64 num_inodes;
        __le64 num_blocks;
        u8 block_size; // actual size = 2^block_size
	u8 pad[4079];
} __attribute__((packed));

struct winterfs_indirect_block_list {
	__le64 blocks[WINTERFS_BLOCK_SIZE / sizeof(__le64)];
} __attribute__((packed));

struct winterfs_inode {
	__le64 size;
        u8 type;
	u8 pad[39]; // reserved for metadata
	winterfs_block_addr primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        winterfs_block_addr secondary_blocks;
	winterfs_block_addr tertiary_blocks;
} __attribute__((packed));

struct winterfs_free_blocks {
	u8 *bitset;
} __attribute__((packed));
