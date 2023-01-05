#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "winterfs.h"
#include "winterfs_dir.h"
#include "winterfs_file.h"
#include "winterfs_ino.h"
#include "winterfs_sb.h"

u32 winterfs_inode_num_blocks(struct inode *inode)
{
	return (inode->i_size / WINTERFS_BLOCK_SIZE) + ((inode->i_size % WINTERFS_BLOCK_SIZE) != 0);
}

u32 winterfs_translate_block_idx(struct inode *inode, u32 block) 
{
	u32 inode_num_blocks;
	u32 idx = 0;
	struct super_block *sb = inode->i_sb;
	struct winterfs_sb_info *sbi = sb->s_fs_info;
	struct winterfs_inode_info *wfs_info = inode->i_private;
	u32 data_blocks_idx = sbi->data_blocks_idx;

	if (!wfs_info) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
		return 0;
	}

	inode_num_blocks = winterfs_inode_num_blocks(inode);
	if (block >= inode_num_blocks) {
		printk(KERN_ERR "Inode block index out of bounds\n");
		return 0;
	}

	if (block < WINTERFS_NUM_BLOCK_IDX_DIRECT) {
		idx = wfs_info->direct_blocks[block];
	} else if (block < WINTERFS_NUM_BLOCK_IDX_DIRECT
			 + WINTERFS_NUM_BLOCK_IDX_IND1) {
		u8 pidx = (block * (WINTERFS_BLOCK_SIZE / 4)) / WINTERFS_BLOCK_SIZE;
		u16 off = (block * (WINTERFS_BLOCK_SIZE / 4)) % WINTERFS_BLOCK_SIZE;
		struct super_block *sb = inode->i_sb;
		struct buffer_head *bh = sb_bread(sb, wfs_info->direct_blocks[pidx]);
		if (!bh) {
			printk(KERN_ERR "Error reading specified data block\n");
			return idx;
		}
		idx = le32_to_cpu(*((__le32 *)bh->b_data+off));
		brelse(bh);
	// TODO
	} else if (block < WINTERFS_NUM_BLOCK_IDX_DIRECT
			 + WINTERFS_NUM_BLOCK_IDX_IND1
			 + WINTERFS_NUM_BLOCK_IDX_IND2) {

	} else if (block < WINTERFS_NUM_BLOCK_IDX_DIRECT
			 + WINTERFS_NUM_BLOCK_IDX_IND1
			 + WINTERFS_NUM_BLOCK_IDX_IND2
			 + WINTERFS_NUM_BLOCK_IDX_IND3) {
	}

	return data_blocks_idx + idx;
}

struct inode *winterfs_new_inode(struct inode *dir)
{
	int i;
	u32 num_bits;
	u32 bidx;
	u32 free_ino;
	u32 num_bitset_blocks;
	struct buffer_head *bh;
	struct inode *inode;
	struct super_block *sb;
	struct winterfs_sb_info *sbi;

	sb = dir->i_sb;
	inode = new_inode(sb);
	if (!inode) {
                return ERR_PTR(-ENOMEM);
	}

	sbi = sb->s_fs_info;
	bidx = sbi->free_inode_bitset_idx;
	num_bitset_blocks = (sbi->num_inodes / (WINTERFS_BLOCK_SIZE * 8)) + ((sbi->num_inodes % (BLOCK_SIZE * 8)) != 0);
	num_bits = 8 * WINTERFS_BLOCK_SIZE;

	for (i = 0; i < num_bitset_blocks; i++) {
		int zero_bit;
		u32 idx;

		idx = bidx + i;
		bh = sb_bread(sb, idx);
		if (!bh) {
			return ERR_PTR(-EIO);
		}

		zero_bit = find_first_zero_bit((unsigned long*)bh->b_data, num_bits);
		if (zero_bit != num_bits) {
			bh->b_data[zero_bit/8] |= (1 << (zero_bit % 8));
			mark_buffer_dirty(bh);
			free_ino = (i * 8 * WINTERFS_BLOCK_SIZE) + zero_bit;
			brelse(bh);
			break;
		}
		brelse(bh);
	}
	if (free_ino == num_bits) {
		printk("Free inode not found\n");
		return ERR_PTR(-ENOMEM);
	}

	inode->i_ino = free_ino;
	mark_inode_dirty(inode);

	return inode;
}

void winterfs_free_inode(struct inode *inode)
{
	// TODO
}

struct inode *winterfs_iget(struct super_block *sb, u32 ino)
{
	struct inode *inode;
	struct winterfs_inode *wfs_inode;
	struct winterfs_inode_info *wfs_info;
	struct buffer_head *bh = NULL;
	int err = 0;

	inode = iget_locked(sb, ino);
	if (!inode) {
		return ERR_PTR(-ENOMEM);
	}
	if (!(inode->i_state & I_NEW)) {
                return inode;
	}

	wfs_inode = winterfs_get_inode(sb, ino, &bh);
	if (IS_ERR(wfs_inode)) {
		err = PTR_ERR(wfs_inode);
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
                inode->i_op = &winterfs_dir_inode_operations;
                inode->i_fop = &winterfs_dir_operations;
	} else {
		err = -EIO;
		goto cleanup;
	}

	unlock_new_inode(inode);

	brelse(bh);

	return inode;

cleanup:
	brelse(bh);
	make_bad_inode(inode);
	iget_failed(inode);
	return ERR_PTR(err);
}

struct winterfs_inode *winterfs_get_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh_out)
{
	struct buffer_head *bh;
	u32 inode_block; 	
	u32 inode_block_idx;
	u16 offset; 

	inode_block = ((ino-1) * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE;
	inode_block_idx = inode_block + WINTERFS_INODES_BLOCK_IDX;
	offset = ((ino-1) * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE;

	bh = sb_bread(sb, inode_block_idx);
	if (!bh) {
		printk(KERN_ERR "Error reading inode block %u\n", inode_block_idx);
		return ERR_PTR(-EIO);
	}

	*bh_out = bh;
	return (struct winterfs_inode *) bh->b_data;
}
