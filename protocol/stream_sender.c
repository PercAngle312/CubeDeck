#include "cubedeck/stream_sender.h"
#include <string.h>

static void put32(uint8_t *dest, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4u; ++i)
        dest[i] = (uint8_t)(value >> (8u * i));
}

int cdtx_init(cdtx_sender *s, cdtx_source reader, void *context,
              uint32_t length, uint32_t crc32)
{
    if (!s || !reader || length > CDST_MAX_BYTES)
        return -1;
    memset(s, 0, sizeof(*s));
    s->read = reader;
    s->context = context;
    s->expected_length = length;
    s->expected_crc32 = crc32;
    s->phase = CDTX_MANIFEST;
    return 0;
}

int cdtx_next(cdtx_sender *s, cdtp_packet *frame)
{
    size_t remaining;
    if (!s || !frame || s->phase == CDTX_FAILED)
        return -1;
    if (s->phase == CDTX_COMPLETE)
        return 0;
    if (s->pending_valid) {
        *frame = s->pending;
        return 1;
    }
    memset(&s->pending, 0, sizeof(s->pending));
    if (s->phase == CDTX_MANIFEST) {
        s->pending.opcode = CDVS_OP_MANIFEST;
        s->pending.payload_length = CDVS_MANIFEST_BYTES;
        put32(s->pending.payload, s->expected_length);
        put32(s->pending.payload + 4u, s->expected_crc32);
    } else if (s->phase == CDTX_FINISH) {
        s->pending.opcode = CDVS_OP_FINISH;
        s->pending.sequence = (uint16_t)s->next_sequence;
    } else if (s->phase == CDTX_DATA) {
        if (s->next_sequence >= CDST_MAX_CHUNKS ||
            s->offset >= s->expected_length)
            return -1;
        remaining = s->expected_length - s->offset;
        s->pending.opcode = CDCS_OP_DATA;
        s->pending.sequence = (uint16_t)s->next_sequence;
        s->pending.payload_length = (uint16_t)(remaining < CDTP_MAX_PAYLOAD ?
                                               remaining : CDTP_MAX_PAYLOAD);
        if (s->read(s->context, s->offset, s->pending.payload,
                    s->pending.payload_length) != 0) {
            memset(&s->pending, 0, sizeof(s->pending));
            return -1;
        }
    } else {
        return -1;
    }
    s->pending_valid = 1;
    *frame = s->pending;
    return 1;
}

int cdtx_timeout(cdtx_sender *s)
{
    if (!s || !s->pending_valid || s->phase == CDTX_FAILED ||
        s->phase == CDTX_COMPLETE)
        return -1;
    if (s->retries >= CDRL_MAX_RETRIES) {
        s->phase = CDTX_FAILED;
        return -1;
    }
    ++s->retries;
    return 0;
}

int cdtx_reply(cdtx_sender *s, const cdtp_packet *reply)
{
    if (!s || !reply || !s->pending_valid ||
        s->phase == CDTX_COMPLETE || s->phase == CDTX_FAILED)
        return -1;
    if (reply->payload_length != 0u ||
        reply->sequence != s->pending.sequence)
        return 0;
    if (reply->opcode == CDRL_OP_NACK)
        return cdtx_timeout(s) == 0 ? 0 : -1;
    if (reply->opcode != CDRL_OP_ACK)
        return 0;
    if (s->phase == CDTX_MANIFEST)
        s->phase = s->expected_length ? CDTX_DATA : CDTX_FINISH;
    else if (s->phase == CDTX_DATA) {
        s->offset += s->pending.payload_length;
        ++s->next_sequence;
        s->phase = s->offset == s->expected_length ? CDTX_FINISH : CDTX_DATA;
    } else if (s->phase == CDTX_FINISH) {
        s->phase = CDTX_COMPLETE;
    }
    s->pending_valid = 0;
    s->retries = 0u;
    return 1;
}
