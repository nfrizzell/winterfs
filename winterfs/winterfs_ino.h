#ifndef WINTERFS_INO
#define WINTERFS_INO

#include <linux/types.h>
#include <linux/fs.h>
#include "winterfs.h"

#define WINTERFS_NULL_INODE		0

#define WINTERFS_INODE_SIZE 		128
#define WINTERFS_INODE_FILE 		0
#define WINTERFS_INODE_DIR 		1

#define WINTERFS_INODE_DIRECT_BLOCKS 	8

#define WINTERFS_NUM_BLOCK_IDX_DIRECT	\
	WINTERFS_INODE_DIRECT_BLOCKS

#define WINTERFS_NUM_BLOCK_IDX_IND1	\
	WINTERFS_BLOCK_SIZE / 4LL 	\

#define WINTERFS_NUM_BLOCK_IDX_IND2	\
	(WINTERFS_BLOCK_SIZE / 16LL)  	\
	* WINTERFS_BLOCK_SIZE

#define WINTERFS_NUM_BLOCK_IDX_IND3	\
	(WINTERFS_BLOCK_SIZE / 64LL) 	\
	* WINTERFS_BLOCK_SIZE	 	\
	* WINTERFS_BLOCK_SIZE

#define WINTERFS_MAX_FILE_SIZE		\
	WINTERFS_INODE_DIRECT_BLOCKS 	\
	* WINTERFS_BLOCK_SIZE		\
					\
	+ WINTERFS_NUM_BLOCK_IDX_IND1	\
	* WINTERFS_BLOCK_SIZE	 	\
					\
	+ WINTERFS_NUM_BLOCK_IDX_IND2	\
	* WINTERFS_BLOCK_SIZE	 	\
					\
	+ WINTERFS_NUM_BLOCK_IDX_IND3	\
	* WINTERFS_BLOCK_SIZE	 	\

#define WINTERFS_TIME_RES 		1000000 // 1 second

struct winterfs_inode {
	__le64 size; // in bytes
	__le64 create_time;
	__le64 modify_time;
	__le64 access_time;
	__le32 parent_ino;
	u8 filename_len; // filenames are placed in their own block
        u8 type;
	u8 pad[46]; // reserved for metadata
	__le32 direct_blocks[WINTERFS_INODE_DIRECT_BLOCKS];
	__le32 indirect_primary;
        __le32 indirect_secondary;
	__le32 indirect_tertiary;

} __attribute__((packed));

// in-memory structure
struct winterfs_inode_info {
        u8 type;
	u32 direct_blocks[WINTERFS_INODE_DIRECT_BLOCKS];
	u32 indirect_primary;
        u32 indirect_secondary;
	u32 indirect_tertiary;
};

extern const struct inode_operations winterfs_file_inode_operations;
extern const struct inode_operations winterfs_dir_inode_operations;

u32 winterfs_inode_num_blocks(struct inode *inode);
u32 winterfs_translate_block_idx(struct inode *inode, u32 block);
struct inode *winterfs_new_inode(struct inode *dir);
struct inode *winterfs_iget (struct super_block *sb, u32 ino);
struct winterfs_inode *winterfs_get_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh_out);

#endif // WINTERFS_INO
