#ifndef WINTERFS_DIR
#define WINTERFS_DIR

#include <linux/types.h>
#include "winterfs.h"
#include "winterfs_sb.h"
#include "winterfs_ino.h"

#define WINTERFS_FILENAME_MAX_LEN	256
#define WINTERFS_FILES_PER_DIR_BLOCK	WINTERFS_BLOCK_SIZE / WINTERFS_FILENAME_MAX_LEN

struct winterfs_dir_inode_list {
	__le32 inode_list[WINTERFS_FILES_PER_DIR_BLOCK-1];
};

struct winterfs_dir_hash_list {
	__le32 hash_list[WINTERFS_FILES_PER_DIR_BLOCK-1];
};

struct winterfs_filename {
	u8 name[WINTERFS_FILENAME_MAX_LEN];
} __attribute__((packed));

struct winterfs_dir_block {
	struct winterfs_dir_inode_list inode_list;
	struct winterfs_dir_hash_list hash_list;
	u8 pad[WINTERFS_FILENAME_MAX_LEN - (sizeof(struct winterfs_dir_inode_list) + sizeof(struct winterfs_dir_hash_list))];
	struct winterfs_filename files[WINTERFS_FILES_PER_DIR_BLOCK-1];
} __attribute__((packed));

struct winterfs_dir_block_info {
	u32 inode_list[WINTERFS_FILES_PER_DIR_BLOCK-1];
	u32 hash_list[WINTERFS_FILES_PER_DIR_BLOCK-1];
	struct winterfs_filename files[WINTERFS_FILES_PER_DIR_BLOCK-1];
	struct winterfs_dir_block *db;
	struct buffer_head *bh;
};

extern const struct file_operations winterfs_dir_operations;

#endif // WINTERFS_DIR
