#include <stdlib.h>
#include "tape_scanner/utils/crc32.h"

static uint32_t crc32_table[256] = {0};

void crc32_init(void)
{
    uint32_t i, j, c;
    for (i = 0; i < 256; i++)
    {
        for (c = i << 24, j = 8; j > 0; --j)
            c = c & 0x80000000 ? (c << 1) ^ 0x04c11db7 : (c << 1);
        crc32_table[i] = c;
    }
}

uint32_t crc32(const char* s, uint32_t len)
{
    uint32_t c = 0, i;
    c ^= 0xFFFFFFFF;
    for (i = 0;  i < len;  i++)
        c = crc32_table[(c ^ s[i]) & 0xFF] ^ ((c >> 8) & 0x00FFFFFF);
    return c ^ 0xFFFFFFFF;
}
