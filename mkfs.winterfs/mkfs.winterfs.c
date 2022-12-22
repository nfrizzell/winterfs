#include <stdio.h>
#include <unistd.h>

int validate_num_blocks(char *arg)
{
	return -1;
}

char *validate_device_path(char *arg)
{
	return NULL;
}

void format_device(int num_blocks, char *device_path)
{

}

int main(int argc, char **argv)
{
	int num_blocks = -1;
	char *device_path = NULL;

	int arg;
	while ((arg = getopt(argc, argv, "bp") != -1)) {
		switch (arg) {
		case 'b':
			num_blocks = validate_num_blocks(optarg);
			break;
		case 'p':
			device_path = validate_device_path(optarg);
			break;
		}
	}

	return 0;
}
