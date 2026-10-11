#ifndef CUBEDECK_RAM_V10_PROTOCOL_H
#define CUBEDECK_RAM_V10_PROTOCOL_H
/* Standalone v10 RAM-only application protocol. No cartridge operations.
 * All values below are logical uint32 words; on the GBA Joybus registers
 * SEND_LO holds bits 0..15 and SEND_HI holds bits 16..31.
 * They are deliberately distinct from the v9 CDR9 (0x39524443) marker. */
#include <stdint.h>

#define CD_V10_READY 0x30524443u /* "CDR0" little-endian */
#define CD_V10_PING  0x47504443u /* "CDPG" little-endian */
#define CD_V10_ACK   0x41504443u /* "CDPA" little-endian */
#define CD_V10_ERROR 0x45504443u /* "CDPE" little-endian */

/* A request frame is exactly 8 bytes. A response is exactly 8 bytes.
 * First uint32: magic; second uint32: sequence. Both are little-endian.
 * Sequence is not a checksum; it binds replies to a single request. */
#define CD_V10_FRAME_BYTES 8u

static inline void cd_v10_store_le32(uint8_t *b, uint32_t v) {
    b[0]=(uint8_t)v; b[1]=(uint8_t)(v>>8);
    b[2]=(uint8_t)(v>>16); b[3]=(uint8_t)(v>>24);
}
static inline uint32_t cd_v10_load_le32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1]<<8) |
           ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}
static inline void cd_v10_encode(uint8_t out[CD_V10_FRAME_BYTES],
                                 uint32_t magic, uint32_t seq) {
    cd_v10_store_le32(out,magic);
    cd_v10_store_le32(out+4,seq);
}
static inline int cd_v10_is_ack(const uint8_t reply[CD_V10_FRAME_BYTES],
                               uint32_t seq) {
    return cd_v10_load_le32(reply)==CD_V10_ACK &&
           cd_v10_load_le32(reply+4)==seq;
}
#endif
