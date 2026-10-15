/* CubeDeck Joyboot RAM payload — experimental, read-only cartridge probe.
 * A multiboot payload executes from GBA work RAM. No Flash/SRAM/EEPROM writes.
 * This is a visual probe; reporting data over Joybus is a later milestone. */
#include <gba.h>
#include <stdint.h>
#include "cubedeck/gba_cart_probe.h"
#include "gba_rom_reader.h"
#include "cubedeck/cart_probe_reply.h"

#define CD_JOYCNT (*(volatile uint16_t *)0x04000140u)
#define CD_RECV_LO (*(volatile uint16_t *)0x04000150u)
#define CD_RECV_HI (*(volatile uint16_t *)0x04000152u)
#define CD_SEND_LO (*(volatile uint16_t *)0x04000154u)
#define CD_SEND_HI (*(volatile uint16_t *)0x04000156u)
static volatile uint32_t published_word;
static void send_word(uint32_t value)
{
    CD_SEND_HI = (uint16_t)(value >> 16);
    CD_SEND_LO = (uint16_t)value;
}
static void serial_irq(void)
{
    uint16_t flags = CD_JOYCNT;
    (void)CD_RECV_HI;
    (void)CD_RECV_LO;
    if ((flags & 3u) != 0u) send_word(published_word);
    CD_JOYCNT = (uint16_t)(0x40u | (flags & 7u));
}
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
    /* Publish status before enabling Joybus. Existing transfer pair remains
     * separate; only a 32-bit success/failure word is exposed. */
    published_word = cd_cart_probe_reply(result == 0);
    irqSet(IRQ_SERIAL, serial_irq);
    REG_RCNT = (uint16_t)((REG_RCNT & 0x3fffu) | 0xc000u);
    send_word(published_word);
    CD_JOYCNT = 0x47u;
    irqEnable(IRQ_SERIAL);
    /* Leave the result visible. Never touch cartridge save address space. */
    for (;;) {
        VBlankIntrWait();
    }
}
