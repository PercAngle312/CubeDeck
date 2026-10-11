#ifndef CUBEDECK_GBA_CART_PROBE_H
#define CUBEDECK_GBA_CART_PROBE_H
#include <stddef.h>
#include <stdint.h>
#include "cubedeck/gba_cart_header.h"
#define CD_GBA_CART_ROM_BASE 0x08000000u
#define CD_GBA_CART_HEADER_LIMIT CD_GBA_HEADER_SIZE
/* Reader callback reads from an offset in the ROM window, never save memory.
 * Implementations MUST return 0 for success, otherwise failure. */
typedef int (*cd_gba_read_byte)(void *ctx, uint32_t offset, uint8_t *value);
typedef struct {
    uint8_t bytes[CD_GBA_HEADER_SIZE];
    cd_gba_cart_header parsed;
} cd_gba_cart_probe;
int cd_gba_probe_header(cd_gba_read_byte reader, void *ctx,
                        cd_gba_cart_probe *probe);
#endif
