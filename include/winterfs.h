#include <linux/types.h>
#include <linux/fs.h>

#define WINTERFS_MAGIC		0x574e4653

#define WINTERFS_BLOCK_SIZE 	4096 // fixed for now...

#define WINTERFS_SUPERBLOCK_LBA		1

#define WINTERFS_INODE_PRIMARY_BLOCKS 	8
#define WINTERFS_INODE_SIZE 		128
#define WINTERFS_INODE_FILE 		0
#define WINTERFS_INODE_DIR 		1

#define WINTERFS_BYTES_PER_INODE	(15 << 1)
#define WINTERFS_INODES_PER_BLOCK	WINTERFS_BLOCK_SIZE / WINTERFS_INODE_SIZE

// superblock on-disk structure
struct winterfs_superblock {
        __le64 num_inodes;
        __le64 num_blocks;
	__le64 first_inode_block;
	__le64 first_data_block;
        u8 block_size; // actual size = 2^block_size
} __attribute__((packed));

// superblock in-memory data
struct winterfs_sb_info {
	u64 num_inodes;
	u64 num_blocks;
	u64 first_inode_block;
	u64 first_data_block;
	struct super_block *vfs_sb;
	spinlock_t s_lock;
	u8 block_size;
};

struct winterfs_indirect_block_list {
	__le32 blocks[WINTERFS_BLOCK_SIZE / sizeof(__le32)];
} __attribute__((packed));

struct winterfs_inode {
	__le64 size;
        u8 type;
	u8 pad[39]; // reserved for metadata
	__le64 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        __le64 secondary_blocks;
	__le64 tertiary_blocks;
} __attribute__((packed));

struct winterfs_inode_info {
	u64 size;
        u8 type;
	u64 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        u64 secondary_blocks;
	u64 tertiary_blocks;
} __attribute__((packed));

struct winterfs_free_blocks {
	u8 *bitset;
} __attribute__((packed));

