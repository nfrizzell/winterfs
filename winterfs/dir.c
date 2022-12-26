#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"

const struct file_operations winterfs_dir_operations = {
	.llseek         = generic_file_llseek,
	.read           = generic_read_dir,
};
