#include "cubedeck/gba_joyboot_words.h"
#include <limits.h>
#include <stdint.h>
/* Derived independently from the MIT-licensed FIX94 GC ROM sender; this
 * module only encodes words and NEVER submits SI transfers. */
static uint32_t swap32(uint32_t x)
{
    return (x << 24) | ((x << 8) & 0x00ff0000u) |
           ((x >> 8) & 0x0000ff00u) | (x >> 24);
}
static uint32_t le32(const uint8_t *src)
{
    return (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
}
static uint32_t crc_step(uint32_t crc, uint32_t value)
{
    unsigned int bit;
    for (bit = 0u; bit < 32u; ++bit) {
        uint32_t mixed = (crc ^ value) & 1u;
        crc = (crc >> 1) ^ (mixed ? 0xa1c1u : 0u);
        value >>= 1;
    }
    return crc;
}
int cd_jb_words_init(cd_jb_words *s, uint32_t raw, size_t size)
{
    if (!s || size < 0x200u || size > 0x3ff40u) return -1;
    s->key = swap32(raw ^ 0x7365646fu);
    s->crc = 0x15a0u;
    s->offset = 0u;
    s->padded_length = ((uint32_t)size + 7u) & ~7u;
    return 0;
}
int cd_jb_next(const uint8_t *image, size_t capacity, cd_jb_words *s,
               uint32_t *out)
{
    uint32_t value, i;
    if (!image || !s || !out || s->offset >= s->padded_length) return -1;
    i = s->offset;
    if (i < 0xc0u) {
        if ((size_t)i + 4u > capacity) return -1;
        value = swap32(le32(image + i));
    } else {
        uint8_t b[4] = {0u, 0u, 0u, 0u};
        unsigned int j;
        for (j = 0u; j < 4u; ++j)
            if ((size_t)i + j < capacity) b[j] = image[i + j];
        value = le32(b);
        s->crc = crc_step(s->crc, value);
        s->key = s->key * 0x6177614bu + 1u;
        value ^= s->key ^ (0xfe000000u - i) ^ 0x20796220u;
    }
    s->offset += 4u;
    *out = value;
    return 0;
}
int cd_jb_finish(cd_jb_words *s, uint32_t *out)
{
    uint32_t result;
    if (!s || !out || s->offset != s->padded_length) return -1;
    result = s->crc | (s->padded_length << 16);
    s->key = s->key * 0x6177614bu + 1u;
    result ^= s->key ^ (0xfe000000u - s->offset) ^ 0x20796220u;
    *out = result;
    return 0;
}
