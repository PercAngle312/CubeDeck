#include "cubedeck/session_sender.h"
#include <string.h>

int cdss_init(cdss_sender *s, const uint8_t *source, size_t length)
{
    if (!s || length > CDCS_MAX_IMAGE || (length && !source))
        return -1;
    memset(s, 0, sizeof(*s));
    s->source = source;
    s->length = length;
    s->crc32 = cdim_crc32(source, length);
    s->phase = CDSS_MANIFEST;
    return 0;
}

int cdss_next(const cdss_sender *s, cdtp_packet *out)
{
    size_t remaining;
    if (!s || !out || s->phase == CDSS_FAILED)
        return -1;
    if (s->phase == CDSS_COMPLETE)
        return 0;
    if (s->phase == CDSS_MANIFEST)
        return cdvs_make_manifest(s->length, s->crc32, out) == 0 ? 1 : -1;
    if (s->phase == CDSS_FINISH)
        return cdvs_make_finish(s->sequence, out) == 0 ? 1 : -1;
    if (s->phase != CDSS_DATA || s->offset >= s->length)
        return -1;
    remaining = s->length - s->offset;
    memset(out, 0, sizeof(*out));
    out->opcode = CDCS_OP_DATA;
    out->sequence = s->sequence;
    out->payload_length = (uint16_t)(remaining < CDTP_MAX_PAYLOAD ?
                                      remaining : CDTP_MAX_PAYLOAD);
    memcpy(out->payload, s->source + s->offset, out->payload_length);
    return 1;
}

int cdss_timeout(cdss_sender *s)
{
    if (!s || s->phase == CDSS_FAILED || s->phase == CDSS_COMPLETE)
        return -1;
    if (s->retries >= CDRL_MAX_RETRIES) {
        s->phase = CDSS_FAILED;
        return -1;
    }
    ++s->retries;
    return 0;
}

int cdss_reply(cdss_sender *s, const cdtp_packet *reply)
{
    uint16_t outstanding;
    cdtp_packet current;
    if (!s || !reply || cdss_next(s, &current) != 1)
        return -1;
    outstanding = current.sequence;
    if (reply->payload_length != 0u || reply->sequence != outstanding)
        return 0;
    if (reply->opcode == CDRL_OP_NACK)
        return cdss_timeout(s) == 0 ? 0 : -1;
    if (reply->opcode != CDRL_OP_ACK)
        return 0;
    s->retries = 0;
    if (s->phase == CDSS_MANIFEST)
        s->phase = s->length ? CDSS_DATA : CDSS_FINISH;
    else if (s->phase == CDSS_DATA) {
        s->offset += current.payload_length;
        ++s->sequence;
        if (s->offset == s->length)
            s->phase = CDSS_FINISH;
    } else if (s->phase == CDSS_FINISH)
        s->phase = CDSS_COMPLETE;
    return 1;
}
