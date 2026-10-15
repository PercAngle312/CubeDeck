#ifndef CUBEDECK_RAM_V11_HEADER_PROTOCOL_H
#define CUBEDECK_RAM_V11_HEADER_PROTOCOL_H
/* EXPERIMENTAL v11 wire contract, NOT connected to hardware.
 * Joybus WRITE and READ each transport exactly one 32-bit word.
 * Each request is an explicit, read-only byte fetch from the first 192
 * bytes of the GBA ROM window. Never access cartridge save memory.
 *
 * Request: [0xD1, transaction ID, byte offset, 0xA5]
 * Reply:   [0xD2, transaction ID, byte offset, byte value]
 * Error:   [0xD3, transaction ID, byte offset, error code]
 *
 * One outstanding request at a time. Transaction IDs rotate and replies
 * must match BOTH the expected transaction ID and offset. No implicit
 * retries on an SI error or timeout: reboot both systems.
 */
#include <stdint.h>
#include "cubedeck/gba_cart_probe.h"
#define CD_V11_REQ_TAG 0xD1u
#define CD_V11_DATA_TAG 0xD2u
#define CD_V11_ERROR_TAG 0xD3u
#define CD_V11_REQUEST_MAGIC 0xA5u
#define CD_V11_ERROR_INVALID 1u
#define CD_V11_ERROR_READ 2u
#define CD_V11_HEADER_LENGTH CD_GBA_CART_HEADER_LIMIT

static inline uint32_t cd_v11_pack(uint8_t tag, uint8_t seq,
                                    uint8_t offset, uint8_t value) {
    return ((uint32_t)tag << 24) | ((uint32_t)seq << 16) |
           ((uint32_t)offset << 8) | value;
}
static inline uint8_t cd_v11_field(uint32_t word, unsigned shift) {
    return (uint8_t)(word >> shift);
}
static inline uint32_t cd_v11_request(uint8_t seq, uint8_t offset) {
    return cd_v11_pack(CD_V11_REQ_TAG, seq, offset, CD_V11_REQUEST_MAGIC);
}
static inline int cd_v11_request_valid(uint32_t word) {
    return cd_v11_field(word,24)==CD_V11_REQ_TAG &&
           cd_v11_field(word,0)==CD_V11_REQUEST_MAGIC &&
           cd_v11_field(word,8)<CD_V11_HEADER_LENGTH;
}
/* Pure GBA responder; callback is invoked only for a valid ROM offset.
 * The caller must invoke this from RAM on the GBA, never on the GameCube. */
static inline uint32_t cd_v11_respond(uint32_t request,
                                       cd_gba_read_byte reader, void *ctx) {
    uint8_t seq=cd_v11_field(request,16);
    uint8_t offset=cd_v11_field(request,8);
    uint8_t value=0;
    if (!cd_v11_request_valid(request))
        return cd_v11_pack(CD_V11_ERROR_TAG,seq,offset,CD_V11_ERROR_INVALID);
    if (!reader || reader(ctx,offset,&value))
        return cd_v11_pack(CD_V11_ERROR_TAG,seq,offset,CD_V11_ERROR_READ);
    return cd_v11_pack(CD_V11_DATA_TAG,seq,offset,value);
}
/* Returns 1 on matching data, 0 on stale/unrelated reply, -1 on a
 * matching explicit error. The caller must separately enforce a deadline. */
static inline int cd_v11_decode(uint32_t reply, uint8_t seq, uint8_t offset,
                                uint8_t *value) {
    uint8_t tag=cd_v11_field(reply,24);
    if (!value || offset>=CD_V11_HEADER_LENGTH) return -1;
    if (cd_v11_field(reply,16)!=seq || cd_v11_field(reply,8)!=offset) return 0;
    if (tag==CD_V11_ERROR_TAG) return -1;
    if (tag!=CD_V11_DATA_TAG) return 0;
    *value=cd_v11_field(reply,0);
    return 1;
}
#endif
