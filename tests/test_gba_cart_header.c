#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/gba_cart_header.h"
int main(void)
{
    uint8_t image[CD_GBA_HEADER_SIZE] = {0};
    cd_gba_cart_header header;
    uint8_t sum = 0u;
    unsigned int i;
    memcpy(image + 0xA0u, "CUBEDECK    ", 12u);
    memcpy(image + 0xACu, "CD01", 4u);
    memcpy(image + 0xB0u, "HB", 2u);
    image[0xBCu] = 7u;
    for (i = 0xA0u; i <= 0xBCu; ++i) sum = (uint8_t)(sum - image[i]);
    image[0xBDu] = (uint8_t)(sum - 0x19u);
    assert(cd_gba_parse_header(image, sizeof(image), &header) == 0);
    assert(header.checksum_valid && header.software_version == 7u);
    assert(strcmp(header.title, "CUBEDECK") == 0);
    assert(strcmp(header.game_code, "CD01") == 0);
    assert(strcmp(header.maker_code, "HB") == 0);
    image[0xA0u] ^= 1u;
    assert(cd_gba_parse_header(image, sizeof(image), &header) == 0);
    assert(!header.checksum_valid);
    assert(cd_gba_parse_header(image, sizeof(image) - 1u, &header) == -1);
    assert(cd_gba_parse_header(NULL, sizeof(image), &header) == -1);
    assert(cd_gba_parse_header(image, sizeof(image), NULL) == -1);
    puts("GBA synthetic header parsing and checksum checks passed.");
    return 0;
}
