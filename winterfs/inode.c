#include <linux/buffer_head.h>
#include <linux/fs.h>
#include "winterfs.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

struct inode *winterfs_new_inode(struct inode *inode, umode_t mode,
	const struct qstr *qstr)
{
	struct super_block *sb;
	struct winterfs_sb_info *sbi;
	struct buffer_head *bbh;
	u32 free_inode;
	u32 bidx;
	u32 num_bitset_blocks;

	sb = inode->i_sb;
	sbi = sb->s_fs_info;
	bidx = sbi->free_inode_bitset_idx;
	num_bitset_blocks = (sbi->num_inodes / (WINTERFS_BLOCK_SIZE * 8)) + ((sbi->num_inodes % (BLOCK_SIZE * 8)) != 0);

	for (int i = 0; i < num_bitset_blocks; i++) {
		int num_bits;
		int zero_bit;

		u32 lba = bidx + i;
		bbh = sb_bread(sb, lba);
		if (!bbh) {
			return ERR_PTR(-EIO);
		}
		num_bits = 8 * WINTERFS_BLOCK_SIZE;
		zero_bit = find_first_zero_bit_le(bbh->b_data, num_bits);
		if (zero_bit != num_bits) {
			bbh->b_data[zero_bit/8] |= (1 << (zero_bit % 8));
			mark_buffer_dirty(bbh);
			free_inode = (i * 8 * WINTERFS_BLOCK_SIZE) + zero_bit;
			brelse(bbh);
			break;
		}
		brelse(bbh);
	}
	if (!free_inode) {
		return ERR_PTR(-ENOMEM);
	}
	return inode;
}

struct inode *winterfs_iget (struct super_block *sb, u64 ino)
{
	struct inode *err;
	struct winterfs_inode *wfs_inode;
	struct winterfs_inode_info *wfs_info;
	struct inode *inode;

	inode = iget_locked(sb, ino);
	if (!inode) {
		return ERR_PTR(-ENOMEM);
	}
	if (!(inode->i_state & I_NEW)) {
                return inode;
	}

	wfs_inode = winterfs_get_inode(sb, ino);
	if (IS_ERR(wfs_inode)) {
		err = ERR_PTR(PTR_ERR(wfs_inode));
		goto cleanup;
	}
	wfs_info = kzalloc(sizeof(struct winterfs_inode_info), GFP_KERNEL);
	inode->i_private = wfs_info;

	inode->i_size = le64_to_cpu(wfs_inode->size);
	inode->i_mode = (wfs_inode->type == WINTERFS_INODE_DIR) ? S_IFDIR : S_IFREG;
	inode->i_atime.tv_sec = le64_to_cpu(wfs_inode->access_time);
	inode->i_mtime.tv_sec = le64_to_cpu(wfs_inode->modify_time);
	inode->i_ctime.tv_sec = le64_to_cpu(wfs_inode->create_time);

	if (S_ISREG(inode->i_mode)) {
                inode->i_op = &winterfs_file_inode_operations;
                inode->i_fop = &winterfs_file_operations;
	} else if (S_ISDIR(inode->i_mode)) {
                inode->i_op = &simple_dir_inode_operations;
                inode->i_fop = &simple_dir_operations;
	} else {
		err = ERR_PTR(-EIO);
		goto cleanup;
	}

	printk(KERN_ERR "Inode info: %llu %d %llu %llu %llu\n", inode->i_size, inode->i_mode, inode->i_atime.tv_sec, inode->i_mtime.tv_sec, inode->i_ctime.tv_sec);

	unlock_new_inode(inode);

	return inode;

cleanup:
	make_bad_inode(inode);
	iget_failed(inode);
	return err;
}

struct winterfs_inode *winterfs_get_inode(struct super_block *sb, ino_t ino)
{
	struct buffer_head *bh;
	struct winterfs_inode *wf_inode;
	u32 inode_block; 	
	u32 inode_block_lba;
	u16 offset; 

	inode_block = ((ino-1) * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE;
	inode_block_lba = inode_block + WINTERFS_INODES_LBA;
	offset = ((ino-1) * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE;
	printk(KERN_ERR "inode_block: %d inode_block_lba: %d offset: %d", inode_block, inode_block_lba, offset);

	bh = sb_bread(sb, inode_block_lba);
	if (!bh) {
		printk(KERN_ERR "Error reading block %u\n", inode_block_lba);
		return ERR_PTR(-EIO);
	}

	wf_inode = kzalloc(sizeof(struct winterfs_inode), GFP_KERNEL);
	if (!wf_inode) {
		return ERR_PTR(-ENOMEM);
	}

	memcpy(wf_inode, bh->b_data + offset, sizeof(struct winterfs_inode));
	brelse(bh);

	return wf_inode;
}
