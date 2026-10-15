#include "gba_rom_reader.h"
#include "cubedeck/gba_cart_probe.h"
#include <stdint.h>
/* The GBA cartridge ROM bus is 16-bit. Use aligned halfword reads rather
 * than byte loads; the latter have device-specific bus behavior. Never
 * address SRAM, Flash, EEPROM or cartridge control registers. */
int cd_gba_rom_header_byte(void *ctx, uint32_t offset, uint8_t *out)
{
    (void)ctx;
    if (!out || offset >= CD_GBA_CART_HEADER_LIMIT) return -1;
#if !defined(__arm__) && !defined(__thumb__)
    /* Refuse accidental host execution: a GBA MMIO address is not mapped. */
    return -1;
#else
    uintptr_t address;
    uint16_t word;
    address = (uintptr_t)CD_GBA_CART_ROM_BASE + (uintptr_t)(offset & ~1u);
    word = *(volatile const uint16_t *)address;
    *out = (uint8_t)((offset & 1u) ? word >> 8 : word);
    return 0;
#endif
}
