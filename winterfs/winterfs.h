#include <linux/fs.h>

#define WINTERFS_INODE_FILE 	0
#define WINTERFS_INODE_DIR 	1

#define WINTERFS_INODE_RATIO	(15 << 1) // how many bytes per inode

struct winterfs_superblock {
        u8 block_size; // actual size = 2^block_size
        __le64 num_inodes;
        __le64 num_blocks;
};

struct winterfs_disk_inode {
        u8 type;
        u8 permissions;
	__le64 size;
};

extern const struct inode_operations winterfs_inode_operations;
