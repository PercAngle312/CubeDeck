#ifndef CUBEDECK_GBA_ROM_READER_H
#define CUBEDECK_GBA_ROM_READER_H
#include <stdint.h>
/* GBA-side callback for cd_gba_probe_header. Must execute on the GBA,
 * from RAM, with a cartridge present. Does not implement multiboot. */
int cd_gba_rom_header_byte(void *ctx, uint32_t offset, uint8_t *out);
#endif
