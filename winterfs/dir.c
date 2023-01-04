#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_dir.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

struct winterfs_filename *winterfs_dir_block_filename(struct winterfs_dir_block_info *db_info, u8 idx)
{
	printk(KERN_ERR "FILENAME: %s\n", db_info->db->files[idx].name);
	return &(db_info->db->files[idx]);
}

static struct winterfs_dir_block_info *winterfs_dir_load_block(struct super_block *sb, u32 block)
{
	u8 i;
	struct buffer_head *bh;
	struct winterfs_dir_block *wdb;
	struct winterfs_dir_block_info *wdbi;

	wdbi = kzalloc(sizeof(struct winterfs_dir_block_info), GFP_KERNEL);
	if (!wdbi) {
		return ERR_PTR(-ENOMEM);
	}

	bh = sb_bread(sb, block);
	if (!bh) {
		printk(KERN_ERR "Error reading block %u\n", block);
		kfree(wdbi);
		return ERR_PTR(-EIO);
	}
	
	wdb = (struct winterfs_dir_block *)bh->b_data;
	wdbi->bh = bh;
	wdbi->db = wdb;
	wdbi->free_bitmap = le16_to_cpu(wdb->free_bitmap);
	for (i = 0; i < WINTERFS_FILES_PER_DIR_BLOCK; i++) {
		wdbi->inode_list[i] = le32_to_cpu(wdb->inode_list[i]);
		printk(KERN_ERR "DIR INODE %d: %d\n", i, wdbi->inode_list[i]);
	}

	return wdbi;
}

static struct dentry *winterfs_lookup(struct inode *dir, struct dentry *dentry, 
	unsigned int flags)
{
	u32 dir_num_blocks;
	u32 block;
	struct winterfs_inode_info *wfs_info;
	struct super_block *sb;
	struct winterfs_sb_info *sbi;

	if (dentry->d_name.len >= WINTERFS_FILENAME_MAX_LEN) {
		return ERR_PTR(-ENAMETOOLONG);
	}

	sb = dir->i_sb;
	sbi = sb->s_fs_info;
	wfs_info = dir->i_private;
	if (!wfs_info) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
		return ERR_PTR(-EINVAL);
	}

	dir_num_blocks = winterfs_inode_num_blocks(dir);
	printk(KERN_ERR "DIR NUM BLOCKS: %d\n", dir_num_blocks);
	for (block = 0; block < dir_num_blocks; block++) {
		u8 file_idx;
		u32 abs_block = block + sbi->data_blocks_idx;
		struct winterfs_dir_block_info *wdbi = winterfs_dir_load_block(sb, abs_block);
		if (IS_ERR(wdbi)) {
			return NULL;
		}
		for (file_idx = 0; file_idx < WINTERFS_FILES_PER_DIR_BLOCK; file_idx++) {
			u32 ino = wdbi->inode_list[file_idx];
			struct winterfs_filename *filename = winterfs_dir_block_filename(wdbi, file_idx);
			printk(KERN_ERR "FILE IDX: %d INO: %d\n", file_idx, ino);
			if (strncmp(filename->name, dentry->d_name.name, WINTERFS_FILENAME_MAX_LEN) == 0) {
				struct inode *inode = winterfs_iget(sb, ino);
				printk(KERN_ERR "MATCH FOUND\n");
				brelse(wdbi->bh);
				kfree(wdbi);
				return d_splice_alias(inode, dentry);
			}
		}
		brelse(wdbi->bh);
		kfree(wdbi);
	}

	printk(KERN_ERR "File not found in dir: %s\n", dentry->d_name.name);
	return NULL;
}

static int winterfs_readdir(struct file *file, struct dir_context *ctx)
{
	u8 i;
	struct winterfs_dir_block_info *wdbi;
	struct inode *inode = file->f_inode;
	struct winterfs_inode_info *wfs_info = inode->i_private;
	struct super_block *sb = inode->i_sb;
	loff_t pos = ctx->pos;
	u8 count = 0;

        if (!wfs_info) {
                printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
                return 0;
        }

	if (pos == 0) {
		u32 double_ino = wfs_info->parent_inode ? wfs_info->parent_inode : inode->i_ino;
		dir_emit(ctx, ".", 1, inode->i_ino, DT_DIR);
		dir_emit(ctx, "..", 2, double_ino, DT_DIR);
	} else if (pos > inode->i_size) {
		return 0;	
	}

	wdbi = winterfs_dir_load_block(sb, pos);
	if (IS_ERR(wdbi)) {
                return 0;
        }
	for (i = 0; i < WINTERFS_FILES_PER_DIR_BLOCK; i++) {
		if (wdbi->inode_list[i] != 0) {
			u32 ino = wdbi->inode_list[i];
			const char *name = (char*)(wdbi->db->files[i].name);
			u32 len = strnlen(name, WINTERFS_FILENAME_MAX_LEN);
			count++;
			dir_emit(ctx, name, len, ino, DT_UNKNOWN);
		}
	}

	ctx->pos += WINTERFS_BLOCK_SIZE;
	brelse(wdbi->bh);
        kfree(wdbi);

	return count;
}

static int winterfs_create(struct user_namespace *mnt_userns, struct inode *dir,
	struct dentry *dentry, umode_t mode, bool excl)
{
	return 0;
}

static int winterfs_mkdir(struct user_namespace *mnt_userns,
        struct inode *dir, struct dentry *dentry, umode_t mode)
{
	struct inode *inode;

	printk(KERN_ERR "MKDIR\n");
	inode = winterfs_new_inode(dir, S_IFDIR | mode, &(dentry->d_name));

	inode->i_op = &winterfs_dir_inode_operations;
        inode->i_fop = &winterfs_dir_operations;

	d_instantiate_new(dentry, inode);

	return 0;
}

const struct inode_operations winterfs_dir_inode_operations = {
	.mkdir		= winterfs_mkdir,
	.create		= winterfs_create,
	.lookup		= winterfs_lookup
};

const struct file_operations winterfs_dir_operations = {
	.iterate	= winterfs_readdir,
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir
};
