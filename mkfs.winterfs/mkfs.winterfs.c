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

#define WINTERFS_BLOCK_SIZE     4096

#define WINTERFS_SUPERBLOCK_LBA         0
#define WINTERFS_INODES_LBA         	1

#define WINTERFS_INODE_PRIMARY_BLOCKS   8
#define WINTERFS_INODE_SIZE             128
#define WINTERFS_INODE_FILE             0
#define WINTERFS_INODE_DIR              1

#define WINTERFS_INODE_RATIO		(1 << 15)

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
	uint64_t create_time;
	uint64_t modify_time;
	uint64_t access_time;
	uint8_t type;
	uint8_t pad[51]; // reserved for metadata
			 
	uint32_t primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
	uint32_t secondary_blocks;
	uint32_t tertiary_blocks;
	uint32_t quaternary_blocks;

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

struct free_blocks {
	uint32_t size;
	uint8_t * bitset;
} __attribute__((packed));

int get_next_free_block(struct free_blocks *fb)
{
	for (size_t i = 0; i < fb->size; i++) {
		if (fb->bitset[i] != 0xFF) {
			int bit = 7;
			while (bit >= 0) {
				if (!((1 << bit) & fb->bitset[i])) {
					fb->bitset[i] |= (1 << bit);
					return (8 * i) + (7 - bit);
				}
				bit--;
			}
		}
	}

	return 0; // err inode
}

int write_superblock(FILE *dev, uint32_t num_blocks, uint32_t num_inodes)
{
	uint32_t num_inode_blocks = ((num_inodes * WINTERFS_INODE_SIZE) / WINTERFS_BLOCK_SIZE) + (((num_inodes * WINTERFS_INODE_SIZE) % WINTERFS_BLOCK_SIZE) != 0);
	uint32_t num_inode_bitset_blocks = (num_inode_blocks / (8 * WINTERFS_BLOCK_SIZE)) + (num_inode_blocks % (8 * WINTERFS_BLOCK_SIZE) != 0);
	uint32_t inode_bitset_idx = num_inode_blocks;

	uint32_t num_block_bitset_blocks = (num_blocks / (8 * WINTERFS_BLOCK_SIZE)) + (num_blocks % (8 * WINTERFS_BLOCK_SIZE) != 0);
	uint32_t free_block_bitset_idx = inode_bitset_idx + num_inode_bitset_blocks;
	uint32_t bad_block_bitset_idx = free_block_bitset_idx + num_block_bitset_blocks;
	uint32_t data_block_idx = bad_block_bitset_idx + num_block_bitset_blocks;

	struct winterfs_superblock sb = {
		.magic = {0x57, 0x4e, 0x46, 0x53},
		.num_blocks = le32(num_blocks), 
		.num_inodes = le32(num_inodes),
		.free_inode_bitset_idx = le32(num_inode_blocks),
		.free_block_bitset_idx = le32(free_block_bitset_idx),
		.bad_block_bitset_idx = le32(bad_block_bitset_idx),
		.data_blocks_idx = le32(data_block_idx),
	};

	fseek(dev, 0, SEEK_SET);
	if(!fwrite(&sb, sizeof(sb), 1, dev)) {
		printf("Failed writing superblock\n");
		return 1;
	}

	return 0;	
}

int write_root_dir_inode(FILE *dev, struct free_blocks *fb)
{
	struct winterfs_inode root = {
		.size = WINTERFS_BLOCK_SIZE,
		.create_time = (uint32_t)time(NULL),
		.modify_time = (uint32_t)time(NULL),
		.access_time = (uint32_t)time(NULL),
		.type = WINTERFS_INODE_DIR,
	};
	root.primary_blocks[0] = get_next_free_block(fb);

	fseek(dev, WINTERFS_BLOCK_SIZE * WINTERFS_INODES_LBA, SEEK_SET);
        if(!fwrite(&root, sizeof(root), 1, dev)) {
                printf("Failed writing root inode\n");
                return 1;
        }

	return 0;
}

int write_free_block_bitset(FILE *dev, struct free_blocks *fb)
{

	return 0;
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

	struct free_blocks fb = {
		.size = num_blocks,
		.bitset = calloc((num_blocks / 8) + (num_blocks % 8 != 0), 1)
	};

	err = write_superblock(dev, num_blocks, num_inodes);
	if (err) {
		goto cleanup;
	}

	err = write_root_dir_inode(dev, &fb);
	if (err) {
		goto cleanup;
	}

	err = write_free_block_bitset(dev, &fb);
	if (err) {
		goto cleanup;
	}

cleanup:
	fclose(dev);
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
