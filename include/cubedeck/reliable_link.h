#ifndef CUBEDECK_RELIABLE_LINK_H
#define CUBEDECK_RELIABLE_LINK_H

#include "cubedeck/chunk_session.h"

/* Stop-and-wait protocol, host-only until transport hardware integration.
 * An ACK carries the sequence accepted by the receiver. A NACK requests
 * retransmission of the outstanding sequence. Timeout/retry clock is owned
 * by caller, not this module; all retries are explicitly bounded.
 */
#define CDRL_OP_ACK  0x21u
#define CDRL_OP_NACK 0x22u
#define CDRL_MAX_RETRIES 3u

typedef enum {
    CDRL_READY = 0,
    CDRL_AWAIT_ACK = 1,
    CDRL_DONE = 2,
    CDRL_FAILED = 3
} cdrl_state;

typedef struct {
    const uint8_t *source;
    size_t length;
    size_t offset;
    uint16_t sequence;
    uint16_t pending_length;
    unsigned int retries;
    cdrl_state state;
} cdrl_sender;

int cdrl_sender_init(cdrl_sender *sender, const uint8_t *source, size_t length);
int cdrl_sender_next(cdrl_sender *sender, cdtp_packet *out);
int cdrl_sender_reply(cdrl_sender *sender, const cdtp_packet *reply);
int cdrl_sender_timeout(cdrl_sender *sender);
int cdrl_make_reply(uint8_t opcode, uint16_t sequence, cdtp_packet *out);
int cdrl_receive_frame(cdcs_receiver *receiver, const uint32_t *words,
                       size_t word_count, cdtp_packet *reply);
#endif
