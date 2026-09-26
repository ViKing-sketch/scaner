#ifndef _CRC32_H
#define _CRC32_H

#include <stdint.h>

void crc32_init(void);
uint32_t crc32(const char* s, uint32_t len);

#endif
