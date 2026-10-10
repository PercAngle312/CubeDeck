#include <gba.h>
#include <stdint.h>
#include "cubedeck/session_guard.h"

/*
 * CubeDeck experimental GBA v2 link diagnostic.
 * Run this normal GBA ROM from a flashcart while connected to GameCube Port 2.
 * It enters Joybus mode, prepares a deterministic response to received words,
 * and never accesses cartridge ROM or save hardware.
 */
#define REG_JOYCNT       (*(volatile uint16_t *)0x04000140)
#define REG_JOY_RECV_L   (*(volatile uint16_t *)0x04000150)
#define REG_JOY_RECV_H   (*(volatile uint16_t *)0x04000152)
#define REG_JOY_TRANS_L  (*(volatile uint16_t *)0x04000154)
#define REG_JOY_TRANS_H  (*(volatile uint16_t *)0x04000156)

#define JOYCNT_RECEIVE (1u << 1) /* Receive-complete flag; write 1 to clear. */
#define JOYCNT_IRQ     (1u << 6)
#define JOY_SIGNATURE  0x4344424Bu /* "CDBK" */
#define JOY_REPLY_XOR  0xA55A3CC3u

static volatile uint32_t received_value;
static volatile uint32_t reply_value = JOY_SIGNATURE;
static volatile uint32_t received_count;
/* IRQ captures packets but retains the proven per-word XOR reply. */
static volatile uint32_t packet_words[CDTP_MAX_PACKET_WORDS];
static volatile unsigned int packet_count;
static volatile unsigned int packet_expected;
static volatile unsigned int packet_ready;

static uint32_t read_received_word(void)
{
    return ((uint32_t)REG_JOY_RECV_H << 16) | REG_JOY_RECV_L;
}

static void write_transmit_word(uint32_t value)
{
    /* Joybus words are exposed as low and high 16-bit registers. */
    REG_JOY_TRANS_H = (uint16_t)(value >> 16);
    REG_JOY_TRANS_L = (uint16_t)value;
}

static void joybus_irq(void)
{
    uint16_t status = REG_JOYCNT;
    if (status & JOYCNT_RECEIVE) {
        uint32_t word = read_received_word();
        received_value = word;
        reply_value = word ^ JOY_REPLY_XOR;
        write_transmit_word(reply_value);
        ++received_count;
        if (!packet_ready) {
            /* Never treat magic inside an active payload as a restart.
             * The portable parser has this invariant too. */
            if (packet_count != 0 || word == CDTP_MAGIC) {
                unsigned int index = packet_count;
                if (index < CDTP_MAX_PACKET_WORDS) {
                    packet_words[index] = word;
                    packet_count = index + 1u;
                    if (packet_count == 3u) {
                        if (word <= CDTP_MAX_PAYLOAD)
                            packet_expected = 4u + ((word + 3u) / 4u);
                        else
                            packet_count = 0;
                    }
                    if (packet_expected && packet_count == packet_expected)
                        packet_ready = 1;
                } else {
                    packet_count = 0;
                    packet_expected = 0;
                }
            }
        }
        REG_JOYCNT |= JOYCNT_RECEIVE;
    }
}

/* Matches the experimental GameCube v2-only test. */
#define CDV2_PROBE_ID 0x42CD2026u

static void paint_screen(uint16_t color)
{
    volatile uint16_t *vram = (volatile uint16_t *)0x06000000;
    for (unsigned int i = 0; i < 240u * 160u; ++i)
        vram[i] = color;
}

int main(void)
{
    irqInit();
    /* RCNT mode bits 15:14 select Joybus mode. */
    REG_RCNT = (uint16_t)((REG_RCNT & 0x3FFFu) | 0xC000u);
    write_transmit_word(JOY_SIGNATURE);
    REG_JOYCNT |= JOYCNT_IRQ;
    irqSet(IRQ_SERIAL, joybus_irq);
    irqEnable(IRQ_SERIAL | IRQ_VBLANK);

    REG_DISPCNT = MODE_3 | BG2_ENABLE;
    paint_screen(RGB5(0, 8, 20));

    uint32_t shown_count = 0;
    uint32_t decoded_words[CDTP_MAX_PACKET_WORDS];
    cdgu_message decoded_packet;
    int packet_result = 0; /* 0 = accumulating, otherwise terminal */
    for (;;) {
        uint32_t count = received_count;
        if (count != shown_count && packet_result == 0) {
            shown_count = count;
            /* Neutral gray indicates raw words arriving, not a finished
             * CRC/session verdict. Error and success colors are reserved
             * exclusively for a completed frame. */
            switch (count & 3u) {
                case 1: paint_screen(RGB5(7, 7, 7)); break;
                case 2: paint_screen(RGB5(12, 12, 12)); break;
                case 3: paint_screen(RGB5(17, 17, 17)); break;
                default: paint_screen(RGB5(22, 22, 22)); break;
            }
        }
        if (packet_ready) {
            unsigned int n, i;
            int status;
            uint16_t old_ime = REG_IME;
            REG_IME = 0;
            n = packet_count;
            for (i = 0; i < n && i < CDTP_MAX_PACKET_WORDS; ++i)
                decoded_words[i] = packet_words[i];
            packet_count = 0;
            packet_expected = 0;
            packet_ready = 0;
            REG_IME = old_ime;
            /* Diagnose each layer separately. "7/7 acknowledged" on
             * GameCube only proves per-word XOR replies, not CRC decoding. */
            {
                cdtp_packet transport;
                status = cdtp_decode(decoded_words, n, &transport);
                if (status != CDTP_OK) {
                    packet_result = -1;
                    if (status == CDTP_ERR_CRC)
                        paint_screen(RGB5(31, 0, 31)); /* magenta: CRC */
                    else
                        paint_screen(RGB5(31, 12, 0)); /* orange: framing */
                } else if (cdgu_decode(decoded_words, n, CDV2_PROBE_ID,
                                       &decoded_packet) != 0) {
                    packet_result = -2;
                    paint_screen(RGB5(0, 0, 31)); /* blue: session or v2 */
                } else if (decoded_packet.opcode != 1u ||
                           decoded_packet.sequence != 1u ||
                           decoded_packet.payload_length != 7u ||
                           decoded_packet.payload[0] != 0x43u ||
                           decoded_packet.payload[1] != 0x44u ||
                           decoded_packet.payload[2] != 0x42u ||
                           decoded_packet.payload[3] != 0x4Bu ||
                           decoded_packet.payload[4] != 0x10u ||
                           decoded_packet.payload[5] != 0x20u ||
                           decoded_packet.payload[6] != 0x30u) {
                    packet_result = -3;
                    paint_screen(RGB5(31, 0, 0)); /* red: app sample */
                } else {
                    packet_result = 1;
                    paint_screen(RGB5(0, 31, 31)); /* cyan: verified */
                }
            }
        }
        VBlankIntrWait();
    }
}
