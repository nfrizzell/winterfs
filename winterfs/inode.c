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

static u32 winterfs_translate_block_idx(struct inode *inode, u32 block) 
{
	u32 inode_num_blocks;
	u32 idx = WINTERFS_NULL_INODE;
	struct winterfs_inode_info *wfs_info = inode->i_private;

	if (!wfs_info) {
		printk(KERN_ERR "Attempt to read data from improperly loaded inode\n");
		return idx;
	}

	inode_num_blocks = winterfs_inode_num_blocks(inode);
	if (block >= inode_num_blocks) {
		printk(KERN_ERR "Inode block index out of bounds\n");
		return idx;
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
		printk(KERN_ERR "DATA BLOCK IDX: %d\n", idx);
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

	return idx;
}

struct inode *winterfs_new_inode(struct inode *inode, umode_t mode,
	const struct qstr *qstr)
{
	int i;
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

	printk(KERN_ERR "New inode stats pt1: bidx: %u num_bitset_blocks: %u\n", bidx, num_bitset_blocks);

	for (i = 0; i < num_bitset_blocks; i++) {
		int num_bits;
		int zero_bit;
		u32 idx;

		idx = bidx + i;
		bbh = sb_bread(sb, idx);
		if (!bbh) {
			return ERR_PTR(-EIO);
		}

		num_bits = 8 * WINTERFS_BLOCK_SIZE;
		zero_bit = find_first_zero_bit_le(bbh->b_data, num_bits);
		if (zero_bit != num_bits) {
			printk(KERN_ERR "New inode stats pt2: num_bits: %u zero_bit: %u idx: %u\n", num_bits, zero_bit, idx);
			bbh->b_data[zero_bit/8] |= (1 << (zero_bit % 8));
			mark_buffer_dirty(bbh);
			free_inode = (i * 8 * WINTERFS_BLOCK_SIZE) + zero_bit;
			brelse(bbh);
			break;
		}
		brelse(bbh);
	}
	if (!free_inode) {
		printk("Zero bit not found\n");
		return ERR_PTR(-ENOMEM);
	}
	return inode;
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
		printk(KERN_ERR "Error reading block %u\n", inode_block_idx);
		return ERR_PTR(-EIO);
	}

	*bh_out = bh;
	return (struct winterfs_inode *) bh->b_data;
}
