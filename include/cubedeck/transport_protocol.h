#ifndef CUBEDECK_TRANSPORT_PROTOCOL_H
#define CUBEDECK_TRANSPORT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Experimental CubeDeck packet framing, version 1.
 *
 * These helpers are host-tested but are not wired into the GameCube runtime
 * or GBA diagnostic yet. Each returned uint32_t is a Joybus data word; its
 * four bytes are transmitted least-significant byte first by the SI layer.
 */
#define CDTP_PROTOCOL_VERSION 1u
#define CDTP_MAX_PAYLOAD 256u
#define CDTP_HEADER_WORDS 3u
#define CDTP_MAX_PAYLOAD_WORDS ((CDTP_MAX_PAYLOAD + 3u) / 4u)
#define CDTP_MAX_PACKET_WORDS (CDTP_HEADER_WORDS + CDTP_MAX_PAYLOAD_WORDS + 1u)

typedef enum {
    CDTP_OK = 0,
    CDTP_ERR_ARGUMENT = -1,
    CDTP_ERR_TOO_LARGE = -2,
    CDTP_ERR_CAPACITY = -3,
    CDTP_ERR_FORMAT = -4,
    CDTP_ERR_CRC = -5
} cdtp_status;

typedef struct {
    uint8_t opcode;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[CDTP_MAX_PAYLOAD];
} cdtp_packet;

/*
 * Word layout:
 *   word 0: ASCII "CDBK" (0x4344424B)
 *   word 1: version[31:24], opcode[23:16], sequence[15:0]
 *   word 2: payload length in bytes
 *   words 3..N: payload bytes packed little-endian, zero-padded to a word
 *   final word: IEEE CRC-32 over the little-endian bytes of words 0..2
 *               followed by the unpadded payload bytes
 */
#define CDTP_MAGIC 0x4344424Bu

cdtp_status cdtp_encode(const cdtp_packet *packet,
                        uint32_t *words,
                        size_t word_capacity,
                        size_t *out_word_count);

cdtp_status cdtp_decode(const uint32_t *words,
                        size_t word_count,
                        cdtp_packet *out_packet);

#ifdef __cplusplus
}
#endif

#endif
