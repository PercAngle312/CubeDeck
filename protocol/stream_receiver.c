#include "cubedeck/stream_receiver.h"
#include <string.h>

static uint32_t load32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t crc_add(uint32_t state, const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        unsigned int bit;
        state ^= data[i];
        for (bit = 0; bit < 8u; ++bit)
            state = (state >> 1) ^ ((state & 1u) ? 0xEDB88320u : 0u);
    }
    return state;
}

void cdst_init(cdst_receiver *s, cdst_sink sink, void *context)
{
    if (s) {
        memset(s, 0, sizeof(*s));
        s->sink = sink;
        s->context = context;
        s->crc_state = 0xFFFFFFFFu;
    }
}

cdst_result cdst_receive(cdst_receiver *s, const cdtp_packet *p)
{
    if (!s || !p || !s->sink)
        return CDST_ERR_ARGUMENT;
    if (p->opcode == CDVS_OP_MANIFEST) {
        uint32_t length, checksum;
        if (p->sequence != 0u || p->payload_length != CDVS_MANIFEST_BYTES)
            return CDST_ERR_STATE;
        length = load32(p->payload);
        checksum = load32(p->payload + 4u);
        if (length > CDST_MAX_BYTES)
            return CDST_ERR_CAPACITY;
        if (s->has_manifest)
            return s->expected_length == length &&
                   s->expected_crc32 == checksum ?
                   CDST_DUPLICATE : CDST_ERR_STATE;
        s->has_manifest = 1;
        s->expected_length = length;
        s->expected_crc32 = checksum;
        return CDST_ACCEPTED;
    }
    if (!s->has_manifest)
        return CDST_ERR_STATE;
    if (p->opcode == CDVS_OP_FINISH) {
        if (p->payload_length != 0u ||
            (uint32_t)p->sequence != s->next_sequence)
            return CDST_ERR_SEQUENCE;
        if (s->offset != s->expected_length ||
            (s->crc_state ^ 0xFFFFFFFFu) != s->expected_crc32)
            return CDST_ERR_INTEGRITY;
        s->verified = 1;
        return CDST_VERIFIED;
    }
    if (p->opcode != CDCS_OP_DATA || s->verified || p->payload_length == 0u)
        return CDST_ERR_STATE;
    if (s->has_last && p->sequence == s->last_sequence) {
        return p->payload_length == s->last_length &&
               memcmp(p->payload, s->last_payload, p->payload_length) == 0 ?
               CDST_DUPLICATE : CDST_ERR_SEQUENCE;
    }
    if (s->next_sequence >= CDST_MAX_CHUNKS ||
        (uint32_t)p->sequence != s->next_sequence)
        return CDST_ERR_SEQUENCE;
    if (s->offset > s->expected_length ||
        p->payload_length > s->expected_length - s->offset)
        return CDST_ERR_CAPACITY;

    /* Callback must commit atomically; no sequence/CRC state advances on error. */
    if (s->sink(s->context, s->offset, p->payload, p->payload_length) != 0)
        return CDST_ERR_SINK;
    s->crc_state = crc_add(s->crc_state, p->payload, p->payload_length);
    s->offset += p->payload_length;
    s->last_sequence = p->sequence;
    s->last_length = p->payload_length;
    memcpy(s->last_payload, p->payload, p->payload_length);
    s->has_last = 1;
    ++s->next_sequence;
    return CDST_ACCEPTED;
}
