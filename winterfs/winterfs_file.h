#ifndef WINTERFS_FILE
#define WINTERFS_FILE

#include <linux/fs.h>

extern const struct file_operations winterfs_file_operations;
extern const struct address_space_operations winterfs_address_operations;

#endif // WINTERFS_FILE
