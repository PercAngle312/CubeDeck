/* RAM-only v9 status payload. No cartridge or storage access. */
#include <gba.h>
#include <stdint.h>
#define JOYCNT (*(volatile uint16_t *)0x04000140u)
#define SEND_LO (*(volatile uint16_t *)0x04000154u)
#define SEND_HI (*(volatile uint16_t *)0x04000156u)
static void publish(void) { SEND_HI=0x3952u; SEND_LO=0x4443u; }
static void serial_irq(void) {
    uint16_t flags=JOYCNT;
    if (flags&3u) publish();
    JOYCNT=(uint16_t)(0x40u|(flags&7u));
}
int main(void) {
    irqInit(); irqEnable(IRQ_VBLANK);
    REG_DISPCNT=MODE_3|BG2_ENABLE;
    volatile uint16_t *vram=(volatile uint16_t *)0x06000000u;
    for (unsigned i=0;i<240u*160u;++i) vram[i]=RGB5(0,31,31);
    irqSet(IRQ_SERIAL,serial_irq);
    REG_RCNT=(uint16_t)((REG_RCNT&0x3fffu)|0xc000u);
    publish(); JOYCNT=0x47u; irqEnable(IRQ_SERIAL);
    for (;;) VBlankIntrWait();
}
