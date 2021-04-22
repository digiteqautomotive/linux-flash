#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "libmtd.h"
#include "crc32.h"
#include "header.h"


#define SN_PART_NAME "mgb4-sn"
#define FW_PART_NAME "mgb4-fw"

#define FPDL3 1
#define GMSL  2

#define ERR_NOT_FOUND     -1
#define ERR_NOT_SPECIFIED -2
#define ERR_IO            -3

#define min(a,b) ((a)<(b)?(a):(b))

static int fw_partition(libmtd_t desc, uint32_t sn)
{
	struct mtd_info info;
	struct mtd_dev_info dev_info;
	int i, fd;
	char mtddev[32];
	int partition = ERR_NOT_FOUND;
	uint32_t psn;


	if (mtd_get_info(desc, &info) < 0) {
		fprintf(stderr, "Error reading MTD info\n");
		return ERR_IO;
	}

	for (i = info.lowest_mtd_num; i <= info.highest_mtd_num; i++) {
		if (mtd_get_dev_info1(desc, i, &dev_info) < 0) {
			fprintf(stderr, "Error getting MTD device #%d info\n", i);
			return ERR_IO;
		}

		if (!strcmp(dev_info.name, FW_PART_NAME)) {
			if (!sn && partition >= 0)
				return ERR_NOT_SPECIFIED;
			partition = i;
			continue;
		}
		if (strcmp(dev_info.name, SN_PART_NAME))
			continue;
		if (partition != i - 1) {
			fprintf(stderr, "Partition order mismatch\n");
			return ERR_IO;
		}

		snprintf(mtddev, sizeof(mtddev), "/dev/mtd%d", i);
		if ((fd = open(mtddev, O_RDONLY)) < 0) {
			fprintf(stderr, "Error opening %s: %s\n", mtddev, strerror(errno));
			return ERR_IO;
		}
		if (mtd_read(&dev_info, fd, 0, 0, &psn, sizeof(psn)) < 0) {
			fprintf(stderr, "Error reading %s\n", mtddev);
			close(fd);
			return ERR_IO;
		}
		close(fd);

		if (sn == psn)
			return partition;
	}

	return  sn ? ERR_NOT_FOUND : partition;
}

static int flash_fw(libmtd_t desc, int partition, const char *data, int size)
{
	struct mtd_dev_info dev_info;
	char mtddev[32];
	int fd;
	int block, ws, offset = 0;

	snprintf(mtddev, sizeof(mtddev), "/dev/mtd%d", partition);
	if ((fd = open(mtddev, O_WRONLY)) < 0) {
		fprintf(stderr, "Error opening %s: %s\n", mtddev, strerror(errno));
		return ERR_IO;
	}

	if (mtd_get_dev_info1(desc, partition, &dev_info) < 0) {
		fprintf(stderr, "Error getting MTD device #%d info\n", partition);
		goto error;
	}

	if (mtd_erase_multi(desc, &dev_info, fd, 0, dev_info.eb_cnt) < 0) {
		fprintf(stderr, "Error erasing %s\n", mtddev);
		goto error;
	}

	while (offset < size) {
		block = offset / dev_info.eb_size;
		ws = min(dev_info.eb_size, size - (block * dev_info.eb_size));
		if (mtd_write(desc, &dev_info, fd, block, 0, (void*)data + offset, ws,
		  0, 0, 0) < 0) {
			fprintf(stderr, "Error writing block #%d to %s\n", block, mtddev);
			goto error;
		}
		offset += dev_info.eb_size;
	}

	close(fd);

	return 0;

error:
	close(fd);

	return ERR_IO;
}

static int read_fw(const char *filename, char **data, size_t *size,
  uint32_t *version)
{
	int fd;
	struct header hdr;
	uint32_t crc, crc_check;
	ssize_t rs;


	if ((fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Error opening input file\n", filename);
		return ERR_IO;
	}

	if (read(fd, &hdr, sizeof(hdr)) < sizeof(hdr) || hdr.magic != FW_MAGIC) {
		fprintf(stderr, "%s: Not a mgb4 FW file\n", filename);
		return ERR_IO;
	}
	if (hdr.size > 0x400000) {
		fprintf(stderr, "%s: %u: Invalid FW data size", filename, hdr.size);
		goto error_fd;
	}

	*size = hdr.size;
	*version = hdr.version;
	crc_check = hdr.crc;
	hdr.crc = 0;

	crc = crc32_init(0);
	crc = crc32_block(crc, (const char *)&hdr, sizeof(hdr));

	if (!(*data = malloc(*size))) {
		fprintf(stderr, "Error allocating FW data memory\n");
		goto error_fd;
	}
	if ((rs = read(fd, *data, *size)) < *size) {
		if (rs < 0)
			fprintf(stderr, "%s: %s\n", filename, strerror(errno));
		else
			fprintf(stderr, "%s: unexpected EOF\n", filename);
		goto error_alloc;
	}

	close(fd);

	crc = crc32_block(crc, *data, *size);
	crc = crc32_finish(crc);

	if (crc != crc_check) {
		fprintf(stderr, "%s: CRC error\n", filename);
		goto error_alloc;
	}

	return 0;

error_alloc:
	free(*data);
error_fd:
	close(fd);

	return ERR_IO;
}

static void usage(const char *cmd)
{
	fprintf(stderr, "Usage: %s [OPTIONS] FILE\n", cmd);
	fprintf(stderr, "Flash the mgb4 card with firmware from FILE.\n\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -s SN    Flash card serial number SN (eg. 0x12345678)\n");
	fprintf(stderr, "  -i       Show firmware info when flashing\n");
}

int main(int argc, char *argv[])
{
	libmtd_t desc;
	uint32_t sn = 0, version;
	int opt, partition, verbose = 0;
	const char *filename, *fw_type;
	char *data;
	size_t size;

	while ((opt = getopt(argc, argv, "his:")) != -1) {
		switch (opt) {
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'i':
				verbose = 1;
				break;
			case 's':
				sn = strtol(optarg, NULL, 16);
				break;
			default: /* '?' */
				usage(argv[0]);
				return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		usage(argv[0]);
		return EXIT_FAILURE;
	} else
		filename = argv[optind];

	if (!(desc = libmtd_open())) {
		if (errno)
			fprintf(stderr, "MTD: %s\n", strerror(errno));
		else
			fprintf(stderr, "MTD not present\n");
		return EXIT_FAILURE;
	}

	if ((partition = fw_partition(desc, sn)) < 0) {
		if (partition == ERR_NOT_FOUND)
			fprintf(stderr, "Card not found\n");
		else if (partition == ERR_NOT_SPECIFIED)
			fprintf(stderr, "Card not specified (multiple cards present)\n");
		goto error_mtd;
	}

	if (read_fw(filename, &data, &size, &version) < 0)
		goto error_mtd;
	if (verbose) {
		fw_type = ((version >> 24) == 1)
		  ? "FPDL3" : ((version >> 24) == 2) ? "GMSL" : "UNKNOWN";
		printf("FW type: %s, version: %u, size: %zu\n", fw_type,
		  version & 0xFFFF, size);
	}
	if (flash_fw(desc, partition, data, size) < 0)
		goto error_data;

	free(data);
	libmtd_close(desc);

	return EXIT_SUCCESS;

error_data:
	free(data);
error_mtd:
	libmtd_close(desc);

	return EXIT_FAILURE;
}
