#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

extern uint32_t crc32_init(uint32_t crc);
extern uint32_t crc32_block(uint32_t crc, const char *buf, size_t len);
extern uint32_t crc32_finish(uint32_t crc);

#endif /* CRC32_H */
