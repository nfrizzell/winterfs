#ifndef WINTERFS_INO
#define WINTERFS_INO

#include <linux/types.h>
#include <linux/fs.h>
#include "winterfs.h"

#define WINTERFS_INODE_PRIMARY_BLOCKS 	8
#define WINTERFS_INODE_SIZE 		128
#define WINTERFS_INODE_FILE 		0
#define WINTERFS_INODE_DIR 		1

#define WINTERFS_MAX_FILE_SIZE	WINTERFS_INODE_PRIMARY_BLOCKS * WINTERFS_BLOCK_SIZE + \
	(WINTERFS_BLOCK_SIZE * WINTERFS_BLOCK_SIZE / 4) + \
	(WINTERFS_BLOCK_SIZE * WINTERFS_BLOCK_SIZE / 16) + \
	(WINTERFS_BLOCK_SIZE / 256 * WINTERFS_BLOCK_SIZE * WINTERFS_BLOCK_SIZE)

struct winterfs_inode {
	__le64 size; // in bytes
	__le64 create_time;
	__le64 modify_time;
	__le64 access_time;
        u8 type;
	u8 pad[51]; // reserved for metadata

	__le32 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        __le32 secondary_blocks;
	__le32 tertiary_blocks;
	__le32 quaternary_blocks;

} __attribute__((packed));

// in-memory structure
struct winterfs_inode_info {
	u64 size;
	u64 create_time;
	u64 modify_time;
	u64 access_time;

        u8 type;
	u32 primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
        u32 secondary_blocks;
	u32 tertiary_blocks;
	u32 quaternary_blocks;
};

extern const struct file_operations winterfs_file_operations;
extern const struct file_operations winterfs_dir_operations;

extern const struct inode_operations winterfs_file_inode_operations;
extern const struct inode_operations winterfs_dir_inode_operations;

struct inode *winterfs_iget (struct super_block *sb, u64 ino);
struct winterfs_inode *winterfs_get_inode(struct super_block *sb, 
	ino_t ino, struct buffer_head **p);

#endif // WINTERFS_INO
