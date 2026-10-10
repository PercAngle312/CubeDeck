#ifndef CUBEDECK_SESSION_GUARD_H
#define CUBEDECK_SESSION_GUARD_H

#include "cubedeck/transport_protocol.h"

/*
 * Host-only v2 wire envelope: every request AND ACK/NACK has an explicit
 * nonzero, caller-selected 32-bit transfer ID. Legacy v1 packets cannot
 * accidentally satisfy this decoder. Session IDs must not be reused while
 * old packets can remain in the link.
 *
 * The first four bytes of a CDTP payload are a little-endian session ID.
 * The remaining 252 bytes are the logical payload. Nothing here writes to
 * cartridge, memory card or filesystem; hardware integration is deferred.
 */
#define CDGU_SESSION_BYTES 4u
#define CDGU_MAX_PAYLOAD (CDTP_MAX_PAYLOAD - CDGU_SESSION_BYTES)

typedef struct {
    uint32_t session_id;
    uint8_t opcode;
    uint16_t sequence;
    uint16_t payload_length;
    uint8_t payload[CDGU_MAX_PAYLOAD];
} cdgu_message;

/* 0 success, -1 invalid arguments/size/zero session ID, -2 format or CRC. */
int cdgu_encode(const cdgu_message *message, uint32_t *words,
                size_t capacity, size_t *out_count);
int cdgu_decode(const uint32_t *words, size_t count,
                uint32_t expected_session, cdgu_message *message);

/* 1 = expected ACK, 0 = stale/unrelated/malformed, -1 = bad arguments.
 * ACK is accepted only for matching session, opcode, and sequence.
 */
int cdgu_ack_matches(const cdgu_message *request,
                     const cdgu_message *ack);
#endif
