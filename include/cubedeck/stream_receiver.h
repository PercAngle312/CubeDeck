#ifndef CUBEDECK_STREAM_RECEIVER_H
#define CUBEDECK_STREAM_RECEIVER_H

#include "cubedeck/verified_session.h"

/* RAM-light host-only receiver for transfers larger than 4 KiB.
 * A sink is responsible for storing or consuming each validated chunk.
 * Sink callback contract: 0 means fully accepted; nonzero means no bytes
 * committed (all-or-nothing). No filesystem/cart/card sink is provided.
 * Sequence space limits one session to 65535 DATA packets.
 */
#define CDST_MAX_CHUNKS 65535u
#define CDST_MAX_BYTES ((uint32_t)CDST_MAX_CHUNKS * CDTP_MAX_PAYLOAD)

typedef int (*cdst_sink)(void *context, size_t offset,
                         const uint8_t *bytes, size_t length);

typedef enum {
    CDST_ACCEPTED = 0,
    CDST_DUPLICATE = 1,
    CDST_VERIFIED = 2,
    CDST_ERR_ARGUMENT = -1,
    CDST_ERR_STATE = -2,
    CDST_ERR_SEQUENCE = -3,
    CDST_ERR_CAPACITY = -4,
    CDST_ERR_INTEGRITY = -5,
    CDST_ERR_SINK = -6
} cdst_result;

typedef struct {
    cdst_sink sink;
    void *context;
    uint32_t expected_length;
    uint32_t expected_crc32;
    uint32_t crc_state;
    size_t offset;
    uint32_t next_sequence;
    uint16_t last_sequence;
    uint16_t last_length;
    uint8_t last_payload[CDTP_MAX_PAYLOAD];
    int has_manifest;
    int has_last;
    int verified;
} cdst_receiver;

void cdst_init(cdst_receiver *receiver, cdst_sink sink, void *context);
cdst_result cdst_receive(cdst_receiver *receiver, const cdtp_packet *packet);
#endif
