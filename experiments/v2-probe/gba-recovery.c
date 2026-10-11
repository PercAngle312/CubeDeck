#include <gba.h>
#include <stdint.h>
#ifdef CUBEDECK_BULK_TRANSFER
#include "bulk_adapter.h"
#else
#include "recovery_packet.h"
#endif

#define CDRP_JOYCNT (*(volatile uint16_t *)0x04000140)
#define CDRP_RECV_L (*(volatile uint16_t *)0x04000150)
#define CDRP_RECV_H (*(volatile uint16_t *)0x04000152)
#define CDRP_TRANS_L (*(volatile uint16_t *)0x04000154)
#define CDRP_TRANS_H (*(volatile uint16_t *)0x04000156)
#define CDRP_RESET_FLAG 1u
#define CDRP_RECEIVE_FLAG 2u
#define CDRP_IRQ_ENABLE 64u
/* Shared only under serial IRQ / REG_IME exclusion. CRC work uses snapshots. */
static cdrp_peer peer;
#ifdef CUBEDECK_RAM16_TRANSFER
/* Avoid a 16 KiB snapshot on the small GBA IWRAM stack. */
static cdrp_work work __attribute__((section(".ewram")));
#endif
static volatile uint32_t changed;
static uint16_t lock_peer(void)
{
    uint16_t previous = REG_IME;
    REG_IME = 0;
    __asm__ __volatile__("" ::: "memory");
    return previous;
}
static void unlock_peer(uint16_t previous)
{
    __asm__ __volatile__("" ::: "memory");
    REG_IME = previous;
}
static void transmit(uint32_t word)
{
    CDRP_TRANS_H = (uint16_t)(word >> 16);
    CDRP_TRANS_L = (uint16_t)word;
}
static void joybus_irq(void)
{
    uint16_t flags = CDRP_JOYCNT;
    if (flags & CDRP_RESET_FLAG) {
        (void)CDRP_RECV_H;
        (void)CDRP_RECV_L;
        cdrp_peer_reset(&peer);
        transmit(peer.reply);
        ++changed;
    } else if (flags & CDRP_RECEIVE_FLAG) {
        uint32_t word = ((uint32_t)CDRP_RECV_H << 16) | CDRP_RECV_L;
        transmit(cdrp_peer_word(&peer, word));
    }
    CDRP_JOYCNT = CDRP_IRQ_ENABLE | (flags & 7u);
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
    cdrp_peer_reset(&peer);
    REG_RCNT = (uint16_t)((REG_RCNT & 0x3FFFu) | 0xC000u);
    transmit(peer.reply);
    CDRP_JOYCNT = CDRP_IRQ_ENABLE | 7u;
    irqEnable(IRQ_SERIAL | IRQ_VBLANK);
    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    uint32_t shown = UINT32_MAX;
    for (;;) {

#ifndef CUBEDECK_RAM16_TRANSFER
        cdrp_work work;
#endif
        cdrp_result result;
        uint16_t old_ime = lock_peer();
        int ready = cdrp_peer_take(&peer, &work);
        unlock_peer(old_ime);
        if (ready) {
            /* Decode CRC/session/opcode/sequence/payload outside serial IRQ.
             * A RESET or FRAME restart can run here and invalidate this work. */
            cdrp_peer_validate(&work, &result);
            old_ime = lock_peer();
            if (cdrp_peer_publish(&peer, &result)) ++changed;
            unlock_peer(old_ime);
        }
        old_ime = lock_peer();
        uint32_t revision = changed, verdict = peer.verdict;
        unsigned int accepted = peer.accepted, forced = peer.forced_nack;
        unsigned int exhausted = peer.exhausted;
        unlock_peer(old_ime);
        if (revision != shown) {
            shown = revision;
            uint16_t color = RGB5(0, 8, 20);
            if (exhausted) color = RGB5(31, 0, 0);
            else if (accepted == CDRP_PACKETS) color = RGB5(0, 31, 31);
            else if ((verdict & 0xFF000000u) == CDRP_NACK)
                color = forced && accepted == 1u ? RGB5(31, 31, 0) : RGB5(31, 0, 0);
            else if (accepted) color = RGB5(0, 31, 0);
            paint(color);
        }
        VBlankIntrWait();
    }
}
