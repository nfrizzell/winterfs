#include <byteswap.h>
#include <errno.h>
#include <linux/fs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>

#define WINTERFS_BLOCK_SIZE     	4096

#define WINTERFS_SUPERBLOCK_BLOCK_ADDR  0

#define WINTERFS_INODE_DIRECT_BLOCKS	8
#define WINTERFS_INODE_SIZE             128

#define WINTERFS_INODE_RATIO		(1 << 15)

#define WINTERFS_DEFAULT_PERMS		0755

bool host_is_le()
{
	int x = 1;
	return *(char *)&x == 1;
}

uint16_t le16(uint16_t val)
{
	if (!host_is_le()) {
		return bswap_16(val);
	}
	return val;
}

uint32_t le32(uint32_t val)
{
	if (!host_is_le()) {
		return bswap_32(val);
	}
	return val;
}

uint64_t le64(uint32_t val)
{
	if (!host_is_le()) {
		return bswap_64(val);
	}
	return val;
}

struct winterfs_inode {
	uint64_t size;
	uint16_t mode;
	uint32_t uid;
	uint32_t gid;
	uint64_t create_time;
	uint64_t modify_time;
	uint64_t access_time;
	uint8_t pad[42]; // reserved for metadata
	uint32_t direct_blocks[WINTERFS_INODE_DIRECT_BLOCKS];
	uint32_t indirect_primary;
	uint32_t indirect_secondary;
	uint32_t indirect_tertiary;
} __attribute__((packed));

struct winterfs_superblock {
	uint8_t magic[4];
	uint32_t num_inodes;
	uint32_t num_blocks;
        uint32_t free_inode_bitset_idx;
        uint32_t free_block_bitset_idx;
        uint32_t bad_block_bitset_idx;
        uint32_t data_blocks_idx;
} __attribute__((packed));

struct winterfs_bitset {
	uint32_t size;
	uint8_t * bitset;
} __attribute__((packed));

int get_next_free_bit(struct winterfs_bitset *bs)
{
	for (size_t i = 0; i < bs->size; i++) {
		if (bs->bitset[i] != 0xFF) {
			int bit = 0;
			while (bit < 8) {
				if (!((1 << bit) & bs->bitset[i])) {
					bs->bitset[i] |= (1 << bit);
					return (8 * i) + bit;
				}
				bit++;
			}
		}
	}

	return 0; // err inode
}

int format_device(char *device_path)
{
	struct stat s;
	int err = stat(device_path, &s);
	if (err) {
		printf("Error opening file: os error %d\n", errno);
		goto err;
	}

	if (!S_ISBLK(s.st_mode)) {
		printf("Not a block device\n");
		goto err;
	}

	FILE *dev = fopen(device_path, "w");
	if (!dev) {
		printf("Error opening file: os error %d\n", errno);
		goto err;
	}

	uint32_t block_dev_size_bytes;
	ioctl(fileno(dev), BLKGETSIZE64, &block_dev_size_bytes);
	uint32_t num_blocks = block_dev_size_bytes / WINTERFS_BLOCK_SIZE;
	uint32_t num_inodes = block_dev_size_bytes / WINTERFS_INODE_RATIO;

	uint32_t num_inode_blocks = ((num_inodes * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE) + (((num_inodes * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE) != 0);

	uint32_t num_inode_bitset_blocks = (num_inode_blocks / (8 * WINTERFS_BLOCK_SIZE)) + (num_inode_blocks % (8 * WINTERFS_BLOCK_SIZE) != 0);
	uint32_t free_inode_bitset_idx = (WINTERFS_SUPERBLOCK_BLOCK_ADDR+1) + num_inode_blocks;

	uint32_t num_block_bitset_blocks = (num_blocks / (8 * WINTERFS_BLOCK_SIZE)) + (num_blocks % (8 * WINTERFS_BLOCK_SIZE) != 0);
	uint32_t free_block_bitset_idx = free_inode_bitset_idx + num_inode_bitset_blocks;
	uint32_t bad_block_bitset_idx = free_block_bitset_idx + num_block_bitset_blocks;
	uint32_t data_block_idx = bad_block_bitset_idx + num_block_bitset_blocks;

	struct winterfs_bitset fi = {
		.size = num_inodes,
		.bitset = calloc((num_inodes / 8) + (num_inodes % 8 != 0), 1)
	};
	// skip null ino
	get_next_free_bit(&fi);

	struct winterfs_bitset fb = {
		.size = num_blocks,
		.bitset = calloc((num_blocks / 8) + (num_blocks % 8 != 0), 1)
	};
	
	struct winterfs_superblock sb = {
		.magic = {0x57, 0x4e, 0x46, 0x53},
		.num_blocks = le32(num_blocks), 
		.num_inodes = le32(num_inodes),
		.free_inode_bitset_idx = le32(free_inode_bitset_idx),
		.free_block_bitset_idx = le32(free_block_bitset_idx),
		.bad_block_bitset_idx = le32(bad_block_bitset_idx),
		.data_blocks_idx = le32(data_block_idx),
	};

	fseek(dev, 0, SEEK_SET);
	if(!fwrite(&sb, sizeof(sb), 1, dev)) {
		printf("Failed writing superblock\n");
		goto cleanup;
	}

	struct winterfs_inode root = {
		.size = le64(WINTERFS_BLOCK_SIZE),
		.mode = S_IFDIR | WINTERFS_DEFAULT_PERMS,
		.create_time = le64((uint32_t)time(NULL)),
		.modify_time = le64((uint32_t)time(NULL)),
		.access_time = le64((uint32_t)time(NULL)),
	};
	root.direct_blocks[0] = get_next_free_bit(&fb);
	get_next_free_bit(&fi);

	fseek(dev, WINTERFS_BLOCK_SIZE * (WINTERFS_SUPERBLOCK_BLOCK_ADDR+1), SEEK_SET);
        if(!fwrite(&root, sizeof(root), 1, dev)) {
                printf("Failed writing root inode\n");
		goto cleanup;
        }

	fseek(dev, WINTERFS_BLOCK_SIZE * free_inode_bitset_idx, SEEK_SET);
	if (!fwrite(fi.bitset, sizeof(fi.bitset), 1, dev)) {
		printf("Failed writing free inode bitset\n");
		goto cleanup;
	}
	
	fseek(dev, WINTERFS_BLOCK_SIZE * free_block_bitset_idx, SEEK_SET);
	if (!fwrite(fb.bitset, sizeof(fi.bitset), 1, dev)) {
		printf("Failed writing free block bitset\n");
		goto cleanup;
	}

cleanup:
	fclose(dev);
	free(fi.bitset);
	free(fb.bitset);
err:
	return err;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Invalid number of arguments\n");
		return 1;
	}

	return format_device(argv[1]);
}
