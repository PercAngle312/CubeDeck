#ifndef CUBEDECK_CHUNK_SESSION_H
#define CUBEDECK_CHUNK_SESSION_H

#include "cubedeck/transport_protocol.h"

/* Host-testable, RAM-only receiver. Does not access cartridge or card storage.
 * A session accepts sequential packets; a repeated prior packet is idempotent
 * only if its payload is byte-for-byte identical.
 */
#define CDCS_OP_DATA 0x20u
#define CDCS_MAX_IMAGE 4096u

typedef enum {
    CDCS_ACCEPTED = 0,
    CDCS_DUPLICATE = 1,
    CDCS_COMPLETE = 2,
    CDCS_ERR_ARGUMENT = -1,
    CDCS_ERR_SEQUENCE = -2,
    CDCS_ERR_CONTENT = -3,
    CDCS_ERR_CAPACITY = -4,
    CDCS_ERR_OPCODE = -5
} cdcs_result;

typedef struct {
    uint8_t data[CDCS_MAX_IMAGE];
    size_t length;
    uint16_t next_sequence;
    uint16_t last_sequence;
    uint16_t last_length;
    uint8_t last_payload[CDTP_MAX_PAYLOAD];
    int has_last;
} cdcs_receiver;

void cdcs_receiver_init(cdcs_receiver *receiver);
cdcs_result cdcs_receive(cdcs_receiver *receiver,
                         const cdtp_packet *packet);
#endif
