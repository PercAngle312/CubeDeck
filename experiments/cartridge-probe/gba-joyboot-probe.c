/* CubeDeck Joyboot RAM payload — experimental, read-only cartridge probe.
 * A multiboot payload executes from GBA work RAM. No Flash/SRAM/EEPROM writes.
 * This is a visual probe; reporting data over Joybus is a later milestone. */
#include <gba.h>
#include <stdint.h>
#include "cubedeck/gba_cart_probe.h"
#include "gba_rom_reader.h"

static cd_gba_cart_probe snapshot __attribute__((section(".ewram")));
static void paint(uint16_t color)
{
    volatile uint16_t *vram = (volatile uint16_t *)0x06000000u;
    unsigned int i;
    for (i = 0u; i < 240u * 160u; ++i) vram[i] = color;
}
int main(void)
{
    int result;
    irqInit();
    irqEnable(IRQ_VBLANK);
    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    /* Blue: running, green: valid header checksum, red: unavailable/invalid. */
    paint(RGB5(0, 0, 20));
    result = cd_gba_probe_header(cd_gba_rom_header_byte, 0, &snapshot);
    paint(result == 0 ? RGB5(0, 24, 0) : RGB5(24, 0, 0));
    /* Leave the result visible. Never touch cartridge save address space. */
    for (;;) {
        VBlankIntrWait();
    }
}
