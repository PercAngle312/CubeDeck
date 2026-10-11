#include <gba.h>
#include <stdint.h>
#include "startup_fixture.h"

#define CDSR_JOYCNT (*(volatile uint16_t *)0x04000140)
#define CDSR_RECV_L (*(volatile uint16_t *)0x04000150)
#define CDSR_RECV_H (*(volatile uint16_t *)0x04000152)
#define CDSR_TRANS_L (*(volatile uint16_t *)0x04000154)
#define CDSR_TRANS_H (*(volatile uint16_t *)0x04000156)
#define CDSR_RESET_FLAG 1u
#define CDSR_RECEIVE_FLAG 2u
#define CDSR_IRQ_ENABLE 64u
static volatile cdsr_peer peer;

static void transmit(uint32_t word)
{
    CDSR_TRANS_H = (uint16_t)(word >> 16);
    CDSR_TRANS_L = (uint16_t)word;
}
static void joybus_irq(void)
{
    uint16_t flags = CDSR_JOYCNT;
    /* RESET supersedes any old receive event, including simultaneous flags.
     * No deferred parser/CRC work can publish a previous-session verdict. */
    cdsr_peer local = peer;
    if (flags & CDSR_RESET_FLAG) {
        /* Drain an old receive register as well as acknowledging JOYCNT;
         * reads clear the hardware receive-pending status in JOYSTAT. */
        (void)CDSR_RECV_H;
        (void)CDSR_RECV_L;
        cdsr_reset(&local);
        transmit(local.reply);
    } else if (flags & CDSR_RECEIVE_FLAG) {
        uint32_t word = ((uint32_t)CDSR_RECV_H << 16) | CDSR_RECV_L;
        cdsr_receive(&local, word);
        transmit(local.reply);
    }
    peer = local;
    /* JOYCNT bits 0..2 are write-one-to-clear: acknowledge only this
     * snapshot, not a read/modify/write that also clears newer events. */
    CDSR_JOYCNT = CDSR_IRQ_ENABLE | (flags & 7u);
}
static void paint(uint16_t color)
{
    volatile uint16_t *vram = (volatile uint16_t *)0x06000000;
    for (unsigned int i = 0; i < 240u * 160u; ++i) vram[i] = color;
}
int main(void)
{
    irqInit();
    irqSet(IRQ_SERIAL, joybus_irq);
    REG_RCNT = (uint16_t)((REG_RCNT & 0x3FFFu) | 0xC000u);
    transmit(CDSR_READY);
    CDSR_JOYCNT = CDSR_IRQ_ENABLE | 7u;
    irqEnable(IRQ_SERIAL | IRQ_VBLANK);
    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    uint32_t shown_resets = UINT32_MAX, shown_received = UINT32_MAX;
    for (;;) {
        uint16_t old_ime = REG_IME;
        REG_IME = 0;
        uint32_t resets = peer.resets, received = peer.received;
        REG_IME = old_ime;
        if (resets != shown_resets || received != shown_received) {
            shown_resets = resets;
            shown_received = received;
            /* Blue: boot/RESET ready. Cyan: synthetic session word received.
             * Cyan is not evidence of packet CRC or cartridge transfer. */
            paint(received ? RGB5(0, 31, 31) : RGB5(0, 8, 20));
        }
        VBlankIntrWait();
    }
}
