#ifndef HEADER_H
#define HEADER_H

#include <stdint.h>

#define FW_MAGIC 0x3462676D

struct header {
	uint32_t magic;
	uint32_t version;
	uint32_t size;
	uint32_t crc;
};

#endif /* HEADER */
