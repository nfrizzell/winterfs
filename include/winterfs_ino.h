#ifndef WINTERFS_INO
#define WINTERFS_INO

#include <linux/types.h>
#include <linux/fs.h>
#include "winterfs.h"

#define WINTERFS_INODE_PRIMARY_BLOCKS 	8
#define WINTERFS_INODE_SIZE 		128
#define WINTERFS_INODE_FILE 		0
#define WINTERFS_INODE_DIR 		1

struct winterfs_inode {
	__le64 size;
        u8 type;
	u8 pad[39]; // reserved for metadata
	__le64 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        __le64 secondary_blocks;
	__le64 tertiary_blocks;
} __attribute__((packed));

// in-memory structure
struct winterfs_inode_info {
	u64 size;
        u8 type;
	u64 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        u64 secondary_blocks;
	u64 tertiary_blocks;
};

#endif // WINTERFS_INO
