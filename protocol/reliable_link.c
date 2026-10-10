#include "cubedeck/reliable_link.h"
#include <string.h>

int cdrl_sender_init(cdrl_sender *s, const uint8_t *source, size_t length)
{
    if (!s || (length && !source) || length > CDCS_MAX_IMAGE)
        return -1;
    memset(s, 0, sizeof(*s));
    s->source = source;
    s->length = length;
    s->state = length ? CDRL_READY : CDRL_DONE;
    return 0;
}

/* Returns 1 for packet available, 0 for no send, -1 for bad args/state. */
int cdrl_sender_next(cdrl_sender *s, cdtp_packet *out)
{
    size_t i;
    if (!s || !out || s->state == CDRL_FAILED)
        return -1;
    if (s->state == CDRL_DONE)
        return 0;
    if (s->state == CDRL_READY) {
        size_t remaining = s->length - s->offset;
        s->pending_length = (uint16_t)(remaining < CDTP_MAX_PAYLOAD ?
                                        remaining : CDTP_MAX_PAYLOAD);
        s->state = CDRL_AWAIT_ACK;
    }
    memset(out, 0, sizeof(*out));
    out->opcode = CDCS_OP_DATA;
    out->sequence = s->sequence;
    out->payload_length = s->pending_length;
    for (i = 0; i < s->pending_length; ++i)
        out->payload[i] = s->source[s->offset + i];
    return 1;
}

/* 1 = accepted, 0 = ignored, -1 = invalid or permanently failed. */
int cdrl_sender_reply(cdrl_sender *s, const cdtp_packet *reply)
{
    if (!s || !reply || s->state != CDRL_AWAIT_ACK)
        return -1;
    if (reply->payload_length != 0u || reply->sequence != s->sequence)
        return 0;
    if (reply->opcode == CDRL_OP_NACK)
        return cdrl_sender_timeout(s) == 0 ? 0 : -1;
    if (reply->opcode != CDRL_OP_ACK)
        return 0;
    s->offset += s->pending_length;
    s->sequence = (uint16_t)(s->sequence + 1u);
    s->retries = 0;
    s->pending_length = 0;
    s->state = s->offset == s->length ? CDRL_DONE : CDRL_READY;
    return 1;
}

int cdrl_sender_timeout(cdrl_sender *s)
{
    if (!s || s->state != CDRL_AWAIT_ACK)
        return -1;
    if (s->retries >= CDRL_MAX_RETRIES) {
        s->state = CDRL_FAILED;
        return -1;
    }
    ++s->retries;
    return 0;
}

int cdrl_make_reply(uint8_t opcode, uint16_t sequence, cdtp_packet *out)
{
    if (!out || (opcode != CDRL_OP_ACK && opcode != CDRL_OP_NACK))
        return -1;
    memset(out, 0, sizeof(*out));
    out->opcode = opcode;
    out->sequence = sequence;
    return 0;
}

/* Decode a complete wire frame, then respond to accepted or replayed data.
 * CRC failures do not update the receiver. Unknown/out-of-order frames
 * yield NACK for receiver's next expected sequence.
 */
int cdrl_receive_frame(cdcs_receiver *receiver, const uint32_t *words,
                       size_t word_count, cdtp_packet *reply)
{
    cdtp_packet decoded;
    cdtp_status status;
    cdcs_result result;
    if (!receiver || !words || !reply)
        return -1;
    status = cdtp_decode(words, word_count, &decoded);
    if (status != CDTP_OK) {
        cdrl_make_reply(CDRL_OP_NACK, receiver->next_sequence, reply);
        return 0;
    }
    result = cdcs_receive(receiver, &decoded);
    if (result == CDCS_ACCEPTED || result == CDCS_DUPLICATE) {
        cdrl_make_reply(CDRL_OP_ACK, decoded.sequence, reply);
        return 1;
    }
    cdrl_make_reply(CDRL_OP_NACK, receiver->next_sequence, reply);
    return 0;
}
