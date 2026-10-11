#include "cubedeck/gba_cart_header.h"
#include <string.h>
static void field(char *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        uint8_t c = src[i];
        dst[i] = (c >= 0x20u && c <= 0x7eu) ? (char)c : '?';
    }
    while (i && dst[i - 1u] == ' ') --i;
    dst[i] = '\0';
}
int cd_gba_parse_header(const uint8_t *bytes, size_t length, cd_gba_cart_header *out)
{
    uint8_t sum = 0u;
    size_t i;
    if (!bytes || !out || length < CD_GBA_HEADER_SIZE) return -1;
    memset(out, 0, sizeof(*out));
    field(out->title, bytes + 0xA0u, 12u);
    field(out->game_code, bytes + 0xACu, 4u);
    field(out->maker_code, bytes + 0xB0u, 2u);
    out->software_version = bytes[0xBCu];
    for (i = 0xA0u; i <= 0xBCu; ++i)
        sum = (uint8_t)(sum - bytes[i]);
    out->expected_checksum = (uint8_t)(sum - 0x19u);
    out->stored_checksum = bytes[0xBDu];
    out->checksum_valid = out->expected_checksum == out->stored_checksum;
    return 0;
}
