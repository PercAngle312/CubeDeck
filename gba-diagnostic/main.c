#include <gba.h>
#include <stdint.h>

/*
 * CubeDeck GBA link diagnostic.
 *
 * Run this .gba program from a GBA flashcart while the GBA is connected to
 * GameCube Port 2 with a GameCube-to-GBA link cable. This is a normal GBA ROM,
 * not a multiboot payload. It uses Joybus mode on the GBA link port.
 *
 * Protocol:
 *   GameCube SI command 0x15 + 32-bit value -> GBA receive interrupt
 *   GameCube SI command 0x14                 -> GBA returns queued 32-bit value
 *
 * This diagnostic never accesses cartridge ROM or save hardware.
 */

#define REG_JOYCNT   (*(volatile uint16_t *)0x04000140)
#define REG_JOY_RECV (*(volatile uint32_t *)0x04000150)
#define REG_JOY_TRANS (*(volatile uint32_t *)0x04000158)
#define REG_RCNT     (*(volatile uint16_t *)0x04000134)

#define JOYCNT_RECEIVE (1u << 1)
#define JOYCNT_SEND    (1u << 2)
#define JOYCNT_IRQ     (1u << 6)
#define JOY_SIGNATURE  0x4344424Bu /* "CDBK" */
#define JOY_REPLY_XOR  0xA55A3CC3u

static volatile uint32_t received_value;
static volatile uint32_t reply_value = JOY_SIGNATURE;
static volatile uint32_t received_count;
static volatile uint32_t sent_count;

static void joybus_irq(void)
{
    uint16_t status = REG_JOYCNT;

    if (status & JOYCNT_RECEIVE) {
        received_value = REG_JOY_RECV;
        ++received_count;
        reply_value = received_value ^ JOY_REPLY_XOR;
        /* Acknowledge the receive request. */
        REG_JOYCNT = JOYCNT_RECEIVE;
    }

    if (status & JOYCNT_SEND) {
        REG_JOY_TRANS = reply_value;
        ++sent_count;
        /* Acknowledge the send request. */
        REG_JOYCNT = JOYCNT_SEND;
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

    /* Set the GBA link port to Joybus mode and enable serial interrupts. */
    REG_RCNT |= (1u << 14) | (1u << 15);
    REG_JOYCNT = JOYCNT_IRQ;
    irqSet(IRQ_SERIAL, joybus_irq);
    irqEnable(IRQ_SERIAL);

    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    paint_screen(RGB5(0, 8, 20));

    for (;;) {
        /* Visual heartbeat: change shade whenever the GC writes a value. */
        uint32_t count = received_count;
        uint16_t shade = (uint16_t)(count & 31u);
        uint16_t color = RGB5(shade, (uint16_t)((count >> 1) & 31u), 20);
        paint_screen(color);
        VBlankIntrWait();
    }
}
