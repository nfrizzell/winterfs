#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"

static int winterfs_get_block(struct inode *inode, sector_t iblock,
        struct buffer_head *bh, int create)
{
	return winterfs_translate_block_idx(inode, iblock);
}

static void winterfs_read_ahead(struct readahead_control *rac)
{
	mpage_readahead(rac, winterfs_get_block);
}

const struct inode_operations winterfs_file_inode_operations = {

};

const struct file_operations winterfs_file_operations = {
	.llseek         = generic_file_llseek,
        .read_iter      = generic_file_read_iter,
        .write_iter     = generic_file_write_iter,
};

const struct address_space_operations winterfs_address_operations = {
	.readahead	= winterfs_read_ahead
};
