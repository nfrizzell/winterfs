#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_dir.h"
#include "winterfs_file.h"
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

static struct dentry *winterfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
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
				winterfs_free_dir_block_info(wdbi, false);
				return d_splice_alias(inode, dentry);
			}
		}
		winterfs_free_dir_block_info(wdbi, false);
	}

	// File not found
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

	if (pos == 0) {
		dir_emit_dot(dir, ctx);
		dir_emit_dotdot(dir, ctx);
	}

	ctx->pos += WINTERFS_BLOCK_SIZE;

	if (pos >= inode->i_size) {
		return count;	
	}

        if (!wfs_info) {
                printk(KERN_ERR "Attempt to read data from improperly loaded inode: %lu\n", inode->i_ino);
                return 0;
        }

	block_idx = pos / WINTERFS_BLOCK_SIZE;
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

	winterfs_free_dir_block_info(wdbi, false);

	return count;
}

static int winterfs_create(struct user_namespace *mnt_userns, struct inode *dir,
	struct dentry *dentry, umode_t mode, bool excl)
{
	int err;
        struct inode *inode;
	struct super_block *sb = dir->i_sb;

        inode = winterfs_new_inode(sb);
        if (IS_ERR(inode)) {
                return PTR_ERR(inode);
	}

	inode->i_op = &winterfs_file_inode_operations;
	inode->i_fop = &winterfs_file_operations;
	inode->i_mapping->a_ops = &winterfs_address_operations;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	inode_init_owner(&init_user_ns, inode, dir, mode);

	d_instantiate_new(dentry, inode);
	err = winterfs_dir_link_inode(dentry, inode);
	if (err) {
		return err;
	}
	__winterfs_write_inode(inode);
        mark_inode_dirty(inode);

	return 0;
}

static int winterfs_unlink(struct inode *dir, struct dentry *dentry)
{
	u32 i;
	u32 num_blocks;
	u32 bitset_block;
	struct buffer_head *bh;
	struct winterfs_dir_block_info *wdbi;
	struct winterfs_inode *wfs_inode;
	struct winterfs_inode_info *wfs_dir_info;
	struct winterfs_inode_info *wfs_file_info;
	struct inode *inode = d_inode(dentry);
	struct super_block *sb = dir->i_sb;
	struct winterfs_sb_info *sbi = sb->s_fs_info;
	int err = 0;

	wfs_dir_info = dir->i_private;
	wfs_file_info = inode->i_private;
	if (!wfs_dir_info || !wfs_file_info) {
                printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
                return -EINVAL;
	}
	
	// remove dir entry
	wdbi = winterfs_dir_load_block(sb, wfs_file_info->dir_block);
	if (IS_ERR(wdbi)) {
                return PTR_ERR(wdbi);
        }
	wdbi->db->inode_list[wfs_file_info->dir_block_off] = WINTERFS_NULL_INODE;
	winterfs_free_dir_block_info(wdbi, true);
	
	// mark inode as free
	bitset_block = sbi->free_inode_bitset_idx + (inode->i_ino / (WINTERFS_BLOCK_SIZE * 8));
	bh = sb_bread(sb, bitset_block);
	if (!bh) {
		printk(KERN_ERR "Error reading directory block %u\n", bitset_block);
		err = -EIO;
		goto finish;
	}
	clear_bit(inode->i_ino % (WINTERFS_BLOCK_SIZE * 8), (unsigned long*)bh->b_data);
	mark_buffer_dirty(bh);
	brelse(bh);

	// mark data blocks as free
	num_blocks = winterfs_inode_num_blocks(inode);
	for (i = 0; i < num_blocks; i++) {
		if (i < WINTERFS_INODE_DIRECT_BLOCKS) {
			u32 translated_block = winterfs_translate_block_idx(inode, i);
			u32 block_num = translated_block - sbi->data_blocks_idx;
			bitset_block = sbi->free_block_bitset_idx + (i / (WINTERFS_BLOCK_SIZE * 8));
			bh = sb_bread(sb, bitset_block);
			if (!bh) {
				printk(KERN_ERR "Error reading directory block %u\n", bitset_block);
				err = -EIO;
				goto finish;
			}
			clear_bit(block_num % (WINTERFS_BLOCK_SIZE * 8), (unsigned long*)bh->b_data);
			mark_buffer_dirty(bh);
			brelse(bh);
		} else {
			//TODO
			break;
		}
        }

finish:
	wfs_dir_info->num_children--;
	mark_inode_dirty(dir);
	mark_inode_dirty(inode);
	inode->i_ctime = dir->i_ctime;
	inode_dec_link_count(inode);
	
	return err;
}

static int winterfs_mkdir(struct user_namespace *mnt_userns,
        struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int err;
	struct inode *inode;
	struct winterfs_inode_info *wfs_info;
	struct super_block *sb = dir->i_sb;

	mode |= S_IFDIR;

	inode_inc_link_count(dir);

	inode = winterfs_new_inode(sb);
	if (IS_ERR(inode)) {
		err = PTR_ERR(inode);
		goto err;
	}

	wfs_info = inode->i_private;
	wfs_info->direct_blocks[0] = winterfs_allocate_data_block(sb);
	
	inode_inc_link_count(inode);

	inode->i_op = &winterfs_dir_inode_operations;
        inode->i_fop = &winterfs_dir_operations;
        inode->i_mapping->a_ops = &winterfs_address_operations;
	inode->i_size = WINTERFS_BLOCK_SIZE;
	inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);
	inode_init_owner(&init_user_ns, inode, dir, mode);

	d_instantiate_new(dentry, inode);
	err = winterfs_dir_link_inode(dentry, inode);
	if (err) {
		goto err;
	}
	__winterfs_write_inode(inode);
	mark_inode_dirty(inode);

	return 0;

err:
	inode_dec_link_count(dir);
	return err;
}

static int winterfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int err;
	struct inode *inode = d_inode(dentry);
	struct winterfs_inode_info *wfs_info = inode->i_private;
	if (!wfs_info) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
                return -EINVAL;
	}

	if (wfs_info->num_children > 0) {
		return -ENOTEMPTY;
	}
		
	err = winterfs_unlink(dir, dentry);
	if (err) {
		return err;
	}

	inode->i_size = 0;
	inode_dec_link_count(inode);
	inode_dec_link_count(dir);

	return 0;
}

void winterfs_free_dir_block_info(struct winterfs_dir_block_info *wdbi, bool dirty)
{
	if (dirty) {
		mark_buffer_dirty(wdbi->bh);
	}
	brelse(wdbi->bh);
	kfree(wdbi);
}

struct winterfs_filename *winterfs_dir_block_filename(
	struct winterfs_dir_block_info *dbi, u8 idx)
{
	return &(dbi->db->files[idx]);
}

int winterfs_dir_link_inode(struct dentry *dent, struct inode *inode)
{
	struct super_block *sb;
	u32 dir_num_blocks;
	u32 block;
	struct winterfs_inode_info *wfs_info_dir;
	struct winterfs_inode_info *wfs_info_file;
	struct inode *dir = d_inode(dent->d_parent);
	int res = -ENOMEM;

	wfs_info_dir = dir->i_private;
	wfs_info_file = inode->i_private;
	if (!wfs_info_dir || !wfs_info_file) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
                return -EINVAL;
	}

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
				wfs_info_dir->num_children++;
				wfs_info_file->dir_block = translated_block;
				wfs_info_file->dir_block_off = i;
				res = 0;	
				break;
			}
		}
		winterfs_free_dir_block_info(wdbi, true);
	}

	mark_inode_dirty(dir);
	//TODO allocate more blocks as needed, write function to do this

	return res;
};

const struct inode_operations winterfs_dir_inode_operations = {
	.mkdir		= winterfs_mkdir,
	.rmdir		= winterfs_rmdir,
	.create		= winterfs_create,
	.lookup		= winterfs_lookup,
	.unlink		= winterfs_unlink
};

const struct file_operations winterfs_dir_operations = {
	.iterate	= winterfs_readdir,
	.llseek		= generic_file_llseek,
	.read		= generic_read_dir
};
