#ifndef WINTERFS_DIR
#define WINTERFS_DIR

#include <linux/types.h>
#include "winterfs.h"
#include "winterfs_sb.h"
#include "winterfs_ino.h"

#define WINTERFS_FILENAME_MAX_LEN	256
#define WINTERFS_FILES_PER_DIR_BLOCK	(WINTERFS_BLOCK_SIZE / WINTERFS_FILENAME_MAX_LEN) - 1

struct winterfs_filename {
	u8 name[WINTERFS_FILENAME_MAX_LEN];
} __attribute__((packed));

struct winterfs_dir_block {
	__le32 inode_list[WINTERFS_FILES_PER_DIR_BLOCK];
	u8 pad[WINTERFS_FILENAME_MAX_LEN - (sizeof(__le32) * 
			(WINTERFS_FILES_PER_DIR_BLOCK))];
	struct winterfs_filename files[WINTERFS_FILES_PER_DIR_BLOCK];
} __attribute__((packed));

struct winterfs_dir_block_info {
	u32 inode_list[WINTERFS_FILES_PER_DIR_BLOCK];
	struct winterfs_dir_block *db;
	struct buffer_head *bh;
};

extern const struct file_operations winterfs_dir_operations;

void winterfs_free_dir_block_info(struct winterfs_dir_block_info *wdbi, bool dirty);
struct winterfs_filename *winterfs_dir_block_filename(
	struct winterfs_dir_block_info *dbi, u8 idx);
int winterfs_dir_link_inode(struct dentry *dent, struct inode *inode);

#endif // WINTERFS_DIR
