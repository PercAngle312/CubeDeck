#ifndef CUBEDECK_RAM_V10_PROTOCOL_H
#define CUBEDECK_RAM_V10_PROTOCOL_H
/* v10 Joybus RAM-only ping protocol. This is not a cart protocol.
 * GameCube WRITE (0x15) delivers one 32-bit little-endian value via the
 * GBA receive registers. READ (0x14) returns one 32-bit value via send
 * registers. There is no atomic 8-byte Joybus frame in this experiment.
 *
 * Bits 31:16 identify the command/reply; bits 15:0 contain its sequence.
 * One request at a time. Do not accept stale ACK for a new sequence. */
#include <stdint.h>
#define CD_V10_READY_WORD 0xCDA00000u
#define CD_V10_PING_TAG   0xCDA10000u
#define CD_V10_ACK_TAG    0xCDA20000u
#define CD_V10_ERROR_TAG  0xCDA30000u
#define CD_V10_TAG_MASK   0xFFFF0000u
#define CD_V10_SEQ_MASK   0x0000FFFFu

static inline uint32_t cd_v10_ping(uint16_t sequence) {
    return CD_V10_PING_TAG | sequence;
}
static inline uint32_t cd_v10_ack(uint16_t sequence) {
    return CD_V10_ACK_TAG | sequence;
}
static inline uint32_t cd_v10_error(uint16_t sequence) {
    return CD_V10_ERROR_TAG | sequence;
}
static inline int cd_v10_is_ping(uint32_t word) {
    return (word & CD_V10_TAG_MASK) == CD_V10_PING_TAG;
}
static inline int cd_v10_is_ack(uint32_t word, uint16_t sequence) {
    return word == cd_v10_ack(sequence);
}
static inline uint16_t cd_v10_sequence(uint32_t word) {
    return (uint16_t)(word & CD_V10_SEQ_MASK);
}
static inline void cd_v10_store_le32(uint8_t out[4], uint32_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8);
    out[2] = (uint8_t)(value >> 16);
    out[3] = (uint8_t)(value >> 24);
}
static inline uint32_t cd_v10_load_le32(const uint8_t in[4]) {
    return (uint32_t)in[0] | ((uint32_t)in[1] << 8) |
           ((uint32_t)in[2] << 16) | ((uint32_t)in[3] << 24);
}
/* Pure GBA responder decision, usable in host tests. */
static inline uint32_t cd_v10_respond(uint32_t request) {
    uint16_t sequence = cd_v10_sequence(request);
    return cd_v10_is_ping(request) ? cd_v10_ack(sequence)
                                   : cd_v10_error(sequence);
}
#endif
