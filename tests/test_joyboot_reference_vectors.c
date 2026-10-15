#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_joyboot_words.h"

static uint32_t bswap(uint32_t x)
{
    return ((x & 0x000000ffu) << 24) |
           ((x & 0x0000ff00u) << 8) |
           ((x & 0x00ff0000u) >> 8) |
           ((x & 0xff000000u) >> 24);
}
static uint32_t reference_crc(uint32_t crc, uint32_t value)
{
    unsigned bit;
    for (bit = 0u; bit != 32u; ++bit) {
        if (((crc ^ value) & 1u) != 0u) crc = (crc >> 1) ^ 0xa1c1u;
        else crc >>= 1;
        value >>= 1;
    }
    return crc;
}
int main(void)
{
    uint8_t image[0x203u];
    cd_jb_words encoder;
    uint32_t wire, expected, key, crc = 0x15a0u;
    uint32_t raw = 0x12345678u;
    uint32_t offset, end = 0x208u;
    unsigned j;
    for (j = 0u; j < sizeof(image); ++j)
        image[j] = (uint8_t)(j ^ 0x5au);
    assert(cd_jb_words_init(&encoder, raw, sizeof(image)) == 0);
    key = bswap(raw ^ 0x7365646fu);
    for (offset = 0u; offset < end; offset += 4u) {
        uint32_t plain = 0u;
        for (j = 0u; j < 4u; ++j) {
            uint32_t index = offset + j;
            if (index < sizeof(image))
                plain |= (uint32_t)image[index] << (8u * j);
        }
        if (offset < 0xc0u) expected = bswap(plain);
        else {
            crc = reference_crc(crc, plain);
            key = key * 0x6177614bu + 1u;
            expected = plain ^ key ^
                       (uint32_t)(0u - (offset + 0x02000000u)) ^ 0x20796220u;
        }
        assert(cd_jb_next(image, sizeof(image), &encoder, &wire) == 0);
        assert(wire == expected);
    }
    assert(encoder.offset == end);
    key = key * 0x6177614bu + 1u;
    expected = (crc | (end << 16)) ^ key ^
               (uint32_t)(0u - (end + 0x02000000u)) ^ 0x20796220u;
    assert(cd_jb_finish(&encoder, &wire) == 0);
    assert(wire == expected);
    assert(cd_jb_next(image, sizeof(image), &encoder, &wire) == -1);
    puts("Joyboot reference vector: every word, 5-byte zero padding and CRC passed.");
    return 0;
}
