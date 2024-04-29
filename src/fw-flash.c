#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <fcntl.h>
#include <unistd.h>
#include "libmtd.h"
#include "crc32.h"
#include "header.h"


#define VERSION "1.1"

#define DATA_PART_NAME "mgb4-data"
#define FW_PART_NAME   "mgb4-fw"

#define FPDL3 1
#define GMSL  2

#define min(a,b) ((a)<(b)?(a):(b))


struct entry {
	int num;
	uint32_t sn;
	int type;
	LIST_ENTRY(entry) entries;
};

LIST_HEAD(list, entry);

static libmtd_t mtd_open()
{
	libmtd_t desc;

	if (!(desc = libmtd_open())) {
		if (errno)
			fprintf(stderr, "MTD: %s\n", strerror(errno));
		else
			fprintf(stderr, "MTD not present\n");
	}

	return desc;
}

static void free_list(struct list *head)
{
	struct entry *n1, *n2;

	n1 = LIST_FIRST(head);
	while (n1 != NULL) {
		n2 = LIST_NEXT(n1, entries);
		free(n1);
		n1 = n2;
	}
}

static int part_list(libmtd_t desc, struct list *head)
{
	struct mtd_info info;
	struct mtd_dev_info dev_info;
	int i, fd, partition = -1;
	char mtddev[32];
	uint32_t sn;
	long long size = 0;
	struct entry *card;


	if (mtd_get_info(desc, &info) < 0) {
		fprintf(stderr, "Error reading MTD info\n");
		return -1;
	}

	for (i = info.lowest_mtd_num; i <= info.highest_mtd_num; i++) {
		if (mtd_get_dev_info1(desc, i, &dev_info) < 0) {
			fprintf(stderr, "Error getting MTD device #%d info\n", i);
			goto error;
		}

		if (!strncmp(dev_info.name, FW_PART_NAME, strlen(FW_PART_NAME))) {
			partition = i;
			size = dev_info.size;
			continue;
		}
		if (strncmp(dev_info.name, DATA_PART_NAME, strlen(DATA_PART_NAME)))
			continue;
		if (partition != i - 1) {
			fprintf(stderr, "Partition order mismatch\n");
			goto error;
		}
		if (size != 0x400000 && size != 0x950000) {
			fprintf(stderr, "Unknown FW partition size (%lld)\n", size);
			goto error;
		}

		snprintf(mtddev, sizeof(mtddev), "/dev/mtd%d", i);
		if ((fd = open(mtddev, O_RDONLY)) < 0) {
			fprintf(stderr, "Error opening %s: %s\n", mtddev, strerror(errno));
			goto error;
		}
		if (mtd_read(&dev_info, fd, 0, 0, &sn, sizeof(sn)) < 0) {
			fprintf(stderr, "Error reading %s\n", mtddev);
			close(fd);
			goto error;
		}
		close(fd);

		if (!(card = malloc(sizeof(struct entry))))
			goto error;
		card->num = partition;
		card->sn = sn;
		card->type = (size == 0x950000) ? 2 : 1;
		LIST_INSERT_HEAD(head, card, entries);
	}

	return 0;

error:
	free_list(head);
	return -1;
}

static int part_find(struct list *head, uint32_t sn, int card_type)
{
	struct entry *np, *fp = 0;
	int cnt = 0;

	LIST_FOREACH(np, head, entries) {
		if (sn == np->sn) {
			if (card_type != np->type) {
				fprintf(stderr, "Card/FW type mismatch\n");
				return -1;
			} else
				return np->num;
		}
		fp = np;
		cnt++;
	}

	if (!sn && cnt == 1) {
		if (card_type != fp->type) {
			fprintf(stderr, "Card/FW type mismatch\n");
			return -1;
		} else
			return fp->num;
	}

	if (sn)
		fprintf(stderr, "0x%x: card not found\n", sn);
	else if (!sn && cnt > 1)
		fprintf(stderr, "Card not specified (multiple cards present)\n");
	else
		fprintf(stderr, "No card found\n");

	return -1;
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
		return -1;
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

	return -1;
}

static int read_fw(const char *filename, char **data, size_t *size,
  uint32_t *version)
{
	int fd;
	struct header hdr;
	uint32_t crc, crc_check;
	ssize_t rs;
	size_t limit;


	if ((fd = open(filename, O_RDONLY)) < 0) {
		fprintf(stderr, "%s: Error opening input file\n", filename);
		return -1;
	}

	if (read(fd, &hdr, sizeof(hdr)) < sizeof(hdr) || hdr.magic != FW_MAGIC) {
		fprintf(stderr, "%s: Not a mgb4 FW file\n", filename);
		return -1;
	}
	limit = (((hdr.version >> 16) & 0xff) <= 1) ? 0x400000 : 0x950000;
	if (hdr.size > limit) {
		fprintf(stderr, "%s: %u: Invalid FW data size\n", filename, hdr.size);
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

	return -1;
}

static int list_devices()
{
	libmtd_t desc;
	struct entry *np;
	struct list head;
	LIST_INIT(&head);


	if (!(desc = mtd_open()))
		return -1;
	if (part_list(desc, &head) < 0)
		goto error_mtd;
	LIST_FOREACH(np, &head, entries)
		printf("%03d-%03d-%03d-%03d (%s)\n", np->sn >> 24, (np->sn >> 16) & 0xFF,
		  (np->sn >>8) & 0xFF, np->sn & 0xFF, np->type == 2 ? "T200" : "T100");
	libmtd_close(desc);
	free_list(&head);

	return 0;

error_mtd:
	libmtd_close(desc);

	return -1;
}

static int str2sn(const char *str, uint32_t *sn)
{
	unsigned b0, b1, b2, b3;

	if (sscanf(str, "%03u-%03u-%03u-%03u", &b3, &b2, &b1, &b0) < 4) {
		fprintf(stderr, "%s: invalid serial number\n", str);
		return -1;
	}

	*sn = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;

	return 0;
}

static void usage(const char *cmd)
{
	fprintf(stderr, "%s - mgb4 firmware flash tool.\n\n", cmd);
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "%s [-s SN] FILE\n", cmd);
	fprintf(stderr, "%s -i FILE\n", cmd);
	fprintf(stderr, "%s -l\n", cmd);
	fprintf(stderr, "%s -v\n\n", cmd);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -s SN    Flash card serial number SN\n");
	fprintf(stderr, "  -i FILE  Show firmware info and exit\n");
	fprintf(stderr, "  -l       List available devices (SNs) and exit\n");
	fprintf(stderr, "  -v       Show program version and exit\n");
}

int main(int argc, char *argv[])
{
	libmtd_t desc;
	uint32_t sn = 0, version;
	int opt, partition, info = 0;
	const char *filename, *fw_type, *card_type;
	char *data;
	size_t size;
	struct list head;

	while ((opt = getopt(argc, argv, "hils:v")) != -1) {
		switch (opt) {
			case 'h':
				usage(argv[0]);
				return EXIT_SUCCESS;
			case 'i':
				info = 1;
				break;
			case 'l':
				return list_devices();
			case 's':
				if (str2sn(optarg, &sn) < 0)
					return EXIT_FAILURE;
				break;
			case 'v':
				printf("%s\n", VERSION);
				return EXIT_SUCCESS;
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

	if (read_fw(filename, &data, &size, &version) < 0)
		return EXIT_FAILURE;
	if (info) {
		fw_type = ((version >> 24) == 1)
		  ? "FPDL3" : ((version >> 24) == 2) ? "GMSL" : "UNKNOWN";
		card_type = (((version >> 16) & 0xff) == 1)
		  ? "T100" : (((version >> 16) & 0xff) == 2) ? "T200" : "UNKNOWN";
		printf("card: %s\ntype: %s\nversion: %u\nsize: %zu\n",
		  card_type, fw_type, version & 0xFFFF, size);
		return EXIT_SUCCESS;
	}

	LIST_INIT(&head);

	if (!(desc = mtd_open()))
		goto error_data;
	if (part_list(desc, &head) < 0)
		goto error_mtd;
	if ((partition = part_find(&head, sn, ((version >> 16) & 0xff))) < 0)
		goto error_list;
	if (flash_fw(desc, partition, data, size) < 0)
		goto error_list;

	free_list(&head);
	free(data);
	libmtd_close(desc);

	return EXIT_SUCCESS;

error_list:
	free_list(&head);
error_mtd:
	libmtd_close(desc);
error_data:
	free(data);

	return EXIT_FAILURE;
}
