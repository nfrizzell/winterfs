#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"

const struct inode_operations winterfs_file_inode_operations = {

};

const struct file_operations winterfs_file_operations = {
	.llseek         = generic_file_llseek,
        .read_iter      = generic_file_read_iter,
        .write_iter     = generic_file_write_iter,
};
