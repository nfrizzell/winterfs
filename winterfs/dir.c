#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_dir.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

static struct winterfs_dir_block_info *winterfs_dir_load_block(struct super_block *sb, 
	u32 block)
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
		printk(KERN_ERR "Error reading directory block %u\n", block);
		kfree(wdbi);
		return ERR_PTR(-EIO);
	}
	
	wdb = (struct winterfs_dir_block *)bh->b_data;
	wdbi->bh = bh;
	wdbi->db = wdb;
	for (i = 0; i < WINTERFS_FILES_PER_DIR_BLOCK; i++) {
		wdbi->inode_list[i] = le32_to_cpu(wdb->inode_list[i]);
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
	for (block = 0; block < dir_num_blocks; block++) {
		struct winterfs_dir_block_info *wdbi;
		u8 file_idx;
		u32 translated_block = winterfs_translate_block_idx(dir, block);
		if (!translated_block) {
			printk(KERN_WARNING "Attempt to access invalid inode block\n");
			continue;
		}
		wdbi = winterfs_dir_load_block(sb, translated_block);
		if (IS_ERR(wdbi)) {
			return NULL;
		}
		for (file_idx = 0; file_idx < WINTERFS_FILES_PER_DIR_BLOCK; file_idx++) {
			u32 ino = wdbi->inode_list[file_idx];
			struct winterfs_filename *filename = winterfs_dir_block_filename(wdbi, file_idx);
			if (strncmp(filename->name, dentry->d_name.name, WINTERFS_FILENAME_MAX_LEN) == 0) {
				struct inode *inode = winterfs_iget(sb, ino);
				winterfs_free_dir_block_info(wdbi);
				return d_splice_alias(inode, dentry);
			}
		}
		winterfs_free_dir_block_info(wdbi);
	}

	printk(KERN_ERR "File not found in dir: %s\n", dentry->d_name.name);
	return NULL;
}

static int winterfs_readdir(struct file *dir, struct dir_context *ctx)
{
	u8 i;
	u32 block_idx;
	u32 translated_idx;
	struct inode *inode = dir->f_inode;
	struct winterfs_dir_block_info *wdbi;
	struct super_block *sb = inode->i_sb;
	struct winterfs_inode_info *wfs_info = inode->i_private;
	loff_t pos = ctx->pos;
	int count = 0;

	dir_emit_dots(dir, ctx);

	if (pos >= inode->i_size) {
		return count;	
	}

        if (!wfs_info) {
                printk(KERN_ERR "Attempt to read data from improperly loaded inode: %lu\n", inode->i_ino);
                return 0;
        }

	block_idx = pos / WINTERFS_BLOCK_SIZE;
	printk(KERN_ERR "block idx: %d", block_idx);
	translated_idx = winterfs_translate_block_idx(inode, block_idx);
	if (!translated_idx) {
		printk(KERN_ERR "Attempt to access invalid inode block: %d\n", translated_idx);
		return count;
	}
	wdbi = winterfs_dir_load_block(sb, translated_idx);
	if (IS_ERR(wdbi)) {
                return count;
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

	winterfs_free_dir_block_info(wdbi);
	ctx->pos += WINTERFS_BLOCK_SIZE;

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
	int err;

	inode_inc_link_count(dir);

	inode = winterfs_new_inode(dir);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto err;
	}

	inode_inc_link_count(inode);

	inode->i_op = &winterfs_dir_inode_operations;
        inode->i_fop = &winterfs_dir_operations;
	inode->i_mode = S_IFDIR;
	inode->i_atime.tv_sec = ktime_get_real();
        inode->i_mtime.tv_sec = ktime_get_real();
        inode->i_ctime.tv_sec = ktime_get_real();

	winterfs_dir_link_inode(dentry, dir, inode);

	d_instantiate_new(dentry, inode);

	return 0;
err:
	inode_dec_link_count(dir);
	return err;
}

void winterfs_free_dir_block_info(struct winterfs_dir_block_info *wdbi)
{
	brelse(wdbi->bh);
	kfree(wdbi);
}

struct winterfs_filename *winterfs_dir_block_filename(
	struct winterfs_dir_block_info *dbi, u8 idx)
{
	return &(dbi->db->files[idx]);
}

int winterfs_dir_link_inode(struct dentry *dent, struct inode *dir,
	struct inode *inode)
{
	struct super_block *sb;
	u32 dir_num_blocks;
	u32 block;
	int res = -ENOMEM;

	sb = dir->i_sb;
	dir_num_blocks = winterfs_inode_num_blocks(dir);
	for (block = 0; (res != 0) && block < dir_num_blocks; block++) {
		u8 i;
		u32 translated_block = winterfs_translate_block_idx(dir, block);
		struct winterfs_dir_block_info *wdbi = winterfs_dir_load_block(sb, translated_block);
		
		for (i = 0; i < WINTERFS_FILES_PER_DIR_BLOCK; i++) {
			if (!wdbi->inode_list[i]) {
				const char *filename = dent->d_name.name;
				wdbi->inode_list[i] = inode->i_ino;
				wdbi->db->inode_list[i] = inode->i_ino;
				strncpy((char*)(&wdbi->db->files[i]), filename, WINTERFS_FILENAME_MAX_LEN);
				mark_buffer_dirty(wdbi->bh);
				res = 0;	
				break;
			}
		}
		winterfs_free_dir_block_info(wdbi);
	}

	//TODO allocate more blocks as needed, write function to do this

	return res;
};


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
