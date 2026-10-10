#include <gba.h>
#include <stdint.h>

/*
 * CubeDeck GBA link diagnostic.
 * Run this normal GBA ROM from a flashcart while connected to GameCube Port 2.
 * It enters Joybus mode, prepares a deterministic response to received words,
 * and never accesses cartridge ROM or save hardware.
 */
#define REG_JOYCNT       (*(volatile uint16_t *)0x04000140)
#define REG_JOY_RECV_L   (*(volatile uint16_t *)0x04000150)
#define REG_JOY_RECV_H   (*(volatile uint16_t *)0x04000152)
#define REG_JOY_TRANS_L  (*(volatile uint16_t *)0x04000154)
#define REG_JOY_TRANS_H  (*(volatile uint16_t *)0x04000156)

#define JOYCNT_RECEIVE (1u << 1)
#define JOYCNT_SEND    (1u << 2)
#define JOYCNT_IRQ     (1u << 6)
#define JOY_SIGNATURE  0x4344424Bu /* "CDBK" */
#define JOY_REPLY_XOR  0xA55A3CC3u

static volatile uint32_t received_value;
static volatile uint32_t reply_value = JOY_SIGNATURE;
static volatile uint32_t received_count;

static uint32_t read_received_word(void)
{
    return ((uint32_t)REG_JOY_RECV_H << 16) | REG_JOY_RECV_L;
}

static void write_transmit_word(uint32_t value)
{
    REG_JOY_TRANS_H = (uint16_t)(value >> 16);
    REG_JOY_TRANS_L = (uint16_t)value;
}

static void joybus_irq(void)
{
    uint16_t status = REG_JOYCNT;
    if (status & JOYCNT_RECEIVE) {
        received_value = read_received_word();
        ++received_count;
        reply_value = received_value ^ JOY_REPLY_XOR;
        REG_JOYCNT |= JOYCNT_RECEIVE;
    }
    if (status & JOYCNT_SEND) {
        write_transmit_word(reply_value);
        REG_JOYCNT |= JOYCNT_SEND;
    }
}

static void paint_screen(uint16_t color)
{
    volatile uint16_t *vram = (volatile uint16_t *)0x06000000;
    for (unsigned int i = 0; i < 240u * 160u; ++i)
        vram[i] = color;
}

int main(void)
{
    irqInit();
    REG_RCNT |= (1u << 14) | (1u << 15);
    REG_JOYCNT |= JOYCNT_IRQ;
    irqSet(IRQ_SERIAL, joybus_irq);
    irqEnable(IRQ_SERIAL | IRQ_VBLANK);

    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    paint_screen(RGB5(0, 8, 20));

    uint32_t shown_count = 0;
    for (;;) {
        uint32_t count = received_count;
        if (count != shown_count) {
            shown_count = count;
            uint16_t shade = (uint16_t)(count & 31u);
            paint_screen(RGB5(shade, (uint16_t)((count >> 1) & 31u), 20));
        }
        VBlankIntrWait();
    }
}
