/* CubeDeck v10 experimental RAM-only ping responder.
 * Separate from the hardware-verified v9 cyan-status program.
 * NO cartridge access, save writes, or storage operations.
 *
 * GameCube WRITE 0x15 supplies a 32-bit word in JOY_RECV.
 * GameCube READ 0x14 retrieves a 32-bit word from JOY_TRANS.
 * The initial READY marker is not an execution ACK; only a sequence-
 * matched response to a real PING counts as a two-way protocol PASS. */
#include <gba.h>
#include <stdint.h>
#include "ram_v10_protocol.h"
#define CD_JOYCNT (*(volatile uint16_t *)0x04000140u)
#define CD_RECV_LO (*(volatile uint16_t *)0x04000150u)
#define CD_RECV_HI (*(volatile uint16_t *)0x04000152u)
#define CD_SEND_LO (*(volatile uint16_t *)0x04000154u)
#define CD_SEND_HI (*(volatile uint16_t *)0x04000156u)
static volatile uint16_t ping_count;
static void publish(uint32_t word) {
    CD_SEND_HI=(uint16_t)(word>>16);
    CD_SEND_LO=(uint16_t)word;
}
static void serial_irq(void) {
    uint16_t flags=CD_JOYCNT;
    if (flags & 1u) {
        ping_count=0;
        publish(CD_V10_READY_WORD);
    }
    if (flags & 2u) {
        uint32_t value=(uint32_t)CD_RECV_LO|((uint32_t)CD_RECV_HI<<16);
        if (cd_v10_is_ping(value)) ++ping_count;
        publish(cd_v10_respond(value));
    }
    CD_JOYCNT=(uint16_t)(0x40u | (flags & 7u));
}
int main(void) {
    irqInit();
    irqEnable(IRQ_VBLANK);
    REG_DISPCNT=MODE_3|BG2_ENABLE;
    volatile uint16_t *vram=(volatile uint16_t *)0x06000000u;
    /* Purple identifies v10 separately from cyan v9. */
    for (unsigned i=0;i<240u*160u;++i) vram[i]=RGB5(20,0,31);
    irqSet(IRQ_SERIAL,serial_irq);
    REG_RCNT=(uint16_t)((REG_RCNT&0x3fffu)|0xc000u);
    publish(CD_V10_READY_WORD);
    CD_JOYCNT=0x47u;
    irqEnable(IRQ_SERIAL);
    for (;;) VBlankIntrWait();
}
