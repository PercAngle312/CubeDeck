#ifndef CUBEDECK_GBA_CART_HEADER_H
#define CUBEDECK_GBA_CART_HEADER_H
#include <stddef.h>
#include <stdint.h>
#define CD_GBA_HEADER_SIZE 192u
typedef struct {
    char title[13];
    char game_code[5];
    char maker_code[3];
    uint8_t software_version;
    uint8_t expected_checksum;
    uint8_t stored_checksum;
    int checksum_valid;
} cd_gba_cart_header;
/* Parse fixed-offset GBA header fields. Returns -1 for incomplete/invalid
 * inputs, 0 for a structurally readable header. A valid checksum alone does
 * not authenticate a cartridge or reveal its ROM size. */
int cd_gba_parse_header(const uint8_t *bytes, size_t length, cd_gba_cart_header *out);
#endif
