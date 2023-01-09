#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"

static int winterfs_get_block(struct inode *inode, sector_t iblock,
        struct buffer_head *bh, int create)
{
	printk(KERN_ERR "iblock: %d bh size: %d\n", iblock, bh->b_size);
	u32 i;
	u32 num_blocks_to_allocate;
	u32 translated_block;
	u32 inode_num_blocks = winterfs_inode_num_blocks(inode);
	struct super_block *sb = inode->i_sb;
	struct winterfs_inode_info *wfs_info = inode->i_private;
	if (!wfs_info) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
		return -EINVAL;
	}
	if (iblock >= inode_num_blocks) {
		num_blocks_to_allocate = (iblock - inode_num_blocks)+1;
		for (i = inode_num_blocks; i <= iblock; i++) {
			if (i < WINTERFS_INODE_DIRECT_BLOCKS) {
				wfs_info->direct_blocks[i] = winterfs_allocate_data_block(sb);
			} else {
				//TODO
				return -ENOMEM;
			}
			printk(KERN_ERR "allocating block %d: %d\n", i, wfs_info->direct_blocks[i]);
			inode->i_size += WINTERFS_BLOCK_SIZE;
		}
	}

	translated_block = winterfs_translate_block_idx(inode, iblock);
	map_bh(bh, inode->i_sb, translated_block);
	bh->b_size = num_blocks_to_allocate * WINTERFS_BLOCK_SIZE;

	mark_inode_dirty(inode);

	return 0;
}

static int winterfs_getattr(struct user_namespace *mnt_userns, const struct path *path,
        struct kstat *stat, u32 request_mask, unsigned int query_flags)
{
	struct inode *inode = d_inode(path->dentry);
	generic_fillattr(&init_user_ns, inode, stat);

	return 0;
}

static int winterfs_setattr(struct user_namespace *mnt_userns, struct dentry *dentry,
	struct iattr *iattr)
{
	int err;
	struct inode *inode = d_inode(dentry);

	err = setattr_prepare(&init_user_ns, dentry, iattr);

	if (iattr->ia_valid & ATTR_SIZE && iattr->ia_size != inode->i_size) {
		err = block_truncate_page(inode->i_mapping, iattr->ia_size, winterfs_get_block);
		if (err) {
			return err;
		}

		truncate_setsize(inode, iattr->ia_size);
		inode->i_mtime = inode->i_ctime = current_time(inode);
	        mark_inode_dirty(inode);
	}

	return 0;
}

static int winterfs_read_folio(struct file *file, struct folio *folio)
{
        return mpage_read_folio(folio, winterfs_get_block);
}

static void winterfs_read_ahead(struct readahead_control *rac)
{
	printk(KERN_ERR "winterfs readahead\n");
	mpage_readahead(rac, winterfs_get_block);
}

static int winterfs_write_page(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, winterfs_get_block, wbc);
}

static int winterfs_write_begin(struct file *file, struct address_space *mapping,
        loff_t pos, unsigned len, struct page **pagep, void **fsdata)
{
        return block_write_begin(mapping, pos, len, pagep, winterfs_get_block);
}

const struct inode_operations winterfs_file_inode_operations = {
	.getattr        = winterfs_getattr,
        .setattr        = winterfs_setattr
};

const struct file_operations winterfs_file_operations = {
	.fsync		= generic_file_fsync,
	.llseek         = generic_file_llseek,
	.mmap		= generic_file_mmap,
	.open		= generic_file_open,
        .read_iter      = generic_file_read_iter,
        .write_iter     = generic_file_write_iter
};

const struct address_space_operations winterfs_address_operations = {
	.readahead		= winterfs_read_ahead,
	.read_folio		= winterfs_read_folio,
	.write_begin		= winterfs_write_begin,
	.writepage		= winterfs_write_page,
	.write_end		= generic_write_end,
	.error_remove_page	= generic_error_remove_page,
};
