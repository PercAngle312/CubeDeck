#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_cart_probe.h"
typedef struct { uint8_t bytes[CD_GBA_HEADER_SIZE]; unsigned int reads; int fail_at; } source;
static int read_byte(void *context, uint32_t offset, uint8_t *out)
{
    source *src = context;
    assert(offset < CD_GBA_HEADER_SIZE);
    if (src->fail_at >= 0 && offset == (uint32_t)src->fail_at) return -1;
    ++src->reads;
    *out = src->bytes[offset];
    return 0;
}
int main(void)
{
    source src = {{0}, 0u, -1};
    cd_gba_cart_probe result, before;
    unsigned int i;
    uint8_t sum = 0u;
    memcpy(src.bytes + 0xA0u, "CUBEDECK    ", 12u);
    memcpy(src.bytes + 0xACu, "CD01", 4u);
    memcpy(src.bytes + 0xB0u, "HB", 2u);
    for (i = 0xA0u; i <= 0xBCu; ++i) sum = (uint8_t)(sum - src.bytes[i]);
    src.bytes[0xBDu] = (uint8_t)(sum - 0x19u);
    assert(cd_gba_probe_header(read_byte, &src, &result) == 0);
    assert(src.reads == 192u && result.parsed.checksum_valid);
    assert(strcmp(result.parsed.game_code, "CD01") == 0);
    before = result;
    src.reads = 0u; src.fail_at = 63;
    assert(cd_gba_probe_header(read_byte, &src, &result) == -1);
    assert(src.reads == 63u && memcmp(&result, &before, sizeof(result)) == 0);
    src.fail_at = -1; src.reads = 0u; src.bytes[0xBDu] ^= 1u;
    assert(cd_gba_probe_header(read_byte, &src, &result) == -1);
    assert(src.reads == 192u && memcmp(&result, &before, sizeof(result)) == 0);
    assert(cd_gba_probe_header(NULL, &src, &result) == -1);
    assert(cd_gba_probe_header(read_byte, &src, NULL) == -1);
    puts("Read-only 192-byte cartridge probe: bounds, checksum and failure atomicity passed.");
    return 0;
}
