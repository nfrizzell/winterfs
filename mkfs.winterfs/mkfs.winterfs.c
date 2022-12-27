#include <byteswap.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define WINTERFS_BLOCK_SIZE     4096

#define WINTERFS_SUPERBLOCK_LBA         1

#define WINTERFS_INODE_PRIMARY_BLOCKS   8
#define WINTERFS_INODE_SIZE             128
#define WINTERFS_INODE_FILE             0
#define WINTERFS_INODE_DIR              1

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
	uint8_t type;
	uint8_t pad[39]; // reserved for metadata
	uint64_t primary_blocks[WINTERFS_INODE_PRIMARY_BLOCKS];
	uint64_t secondary_blocks;
	uint64_t tertiary_blocks;
} __attribute__((packed));

struct winterfs_block_list {
	uint64_t size; // number of items in the list
	uint64_t num_nodes; // number of blocks the list occupies
	uint64_t list_head; // block index of the first node
	uint64_t list_tail; // block index of the last node
} __attribute__((packed));

struct winterfs_free_list_node {
	uint64_t list[(WINTERFS_BLOCK_SIZE / sizeof(uint64_t)) - 1];
	uint64_t next;
} __attribute__((packed));

struct winterfs_inode_list_node {
	struct winterfs_inode list[(WINTERFS_BLOCK_SIZE / WINTERFS_INODE_SIZE) - 1];
	uint8_t pad[WINTERFS_INODE_SIZE - sizeof(uint64_t)];
	uint64_t next;
} __attribute__((packed));

struct winterfs_superblock {
	uint8_t magic[4];
	uint64_t num_inodes;
	uint64_t num_blocks;

	struct winterfs_block_list free_block_list;
	struct winterfs_block_list inode_list;

	uint8_t block_size; // actual size = 2^block_size
} __attribute__((packed));

int write_superblock(FILE *dev, struct stat *s)
{
	struct winterfs_superblock sb = {
		.magic = {0x57, 0x4e, 0x46, 0x53},
		.num_blocks = s->st_size / WINTERFS_BLOCK_SIZE + (s->st_size % WINTERFS_BLOCK_SIZE != 0),
		.num_inodes = 31
	};

	fseek(dev, 0, SEEK_SET);
	if(!fwrite(&sb, sizeof(sb), 1, dev)) {
		printf("Failed writing superblock\n");
		return 1;
	}

	return 0;	
}


int write_free_block_stack(FILE *dev)
{
	return 0;
}

int format_device(char *device_path)
{
	struct stat s;
	int err;

	err = stat(device_path, &s);
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

	err = write_superblock(dev, &s);
	if (err) {
		goto err_file;
	}

	err = write_free_block_stack(dev);
	if (err) {
		goto err_file;
	}

err_file:
	fclose(dev);
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
