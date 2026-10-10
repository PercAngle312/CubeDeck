#include "cubedeck/session_guard.h"
#include "cubedeck/reliable_link.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int roundtrip_all_sizes(void)
{
    cdgu_message source, result, ack;
    uint32_t words[CDTP_MAX_PACKET_WORDS], altered[CDTP_MAX_PACKET_WORDS];
    size_t length, count, i;
    for (length = 0u; length <= CDGU_MAX_PAYLOAD; ++length) {
        memset(&source, 0, sizeof(source));
        source.session_id = 0xF19AC321u;
        source.opcode = 0x20u;
        source.sequence = (uint16_t)length;
        source.payload_length = (uint16_t)length;
        for (i = 0u; i < length; ++i)
            source.payload[i] = (uint8_t)(i * 31u + length);
        CHECK(cdgu_encode(&source, words, CDTP_MAX_PACKET_WORDS,
                          &count) == 0);
        CHECK(cdgu_decode(words, count, source.session_id, &result) == 0);
        CHECK(result.session_id == source.session_id);
        CHECK(result.opcode == source.opcode);
        CHECK(result.sequence == source.sequence);
        CHECK(result.payload_length == length);
        CHECK(memcmp(result.payload, source.payload, length) == 0);
        CHECK(cdgu_decode(words, count, source.session_id + 1u,
                          &result) == -2);
        memcpy(altered, words, count * sizeof(words[0]));
        altered[count - 1u] ^= 1u;
        CHECK(cdgu_decode(altered, count, source.session_id, &result) == -2);
        memset(&ack, 0, sizeof(ack));
        ack.session_id = source.session_id;
        ack.opcode = CDRL_OP_ACK;
        ack.sequence = source.sequence;
        ack.payload_length = 1u;
        ack.payload[0] = source.opcode;
        CHECK(cdgu_ack_matches(&source, &ack) == 1);
        CHECK(cdgu_encode(&ack, words, CDTP_MAX_PACKET_WORDS, &count) == 0);
        CHECK(cdgu_decode(words, count, source.session_id, &result) == 0);
        CHECK(cdgu_ack_matches(&source, &result) == 1);
        ack.session_id ^= 0x100u;
        CHECK(cdgu_ack_matches(&source, &ack) == 0);
        ack.session_id = source.session_id;
        ack.sequence ^= 1u;
        CHECK(cdgu_ack_matches(&source, &ack) == 0);
        ack.sequence = source.sequence;
        ack.payload[0] ^= 1u;
        CHECK(cdgu_ack_matches(&source, &ack) == 0);
        ack.payload[0] = source.opcode;
        ack.opcode = CDRL_OP_NACK;
        CHECK(cdgu_ack_matches(&source, &ack) == 0);
    }
    return 0;
}

static int stale_session_replay(void)
{
    cdgu_message request, ack, received, previous;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    memset(&request, 0, sizeof(request));
    request.session_id = 0xABCD0001u;
    request.opcode = 0x30u;
    CHECK(cdgu_encode(&request, words, CDTP_MAX_PACKET_WORDS,
                      &count) == 0);
    CHECK(cdgu_decode(words, count, 0xABCD0001u, &received) == 0);
    CHECK(cdgu_decode(words, count, 0xABCD0002u, &received) == -2);
    memset(&ack, 0, sizeof(ack));
    ack.session_id = request.session_id;
    ack.opcode = CDRL_OP_ACK;
    ack.payload_length = 1u;
    ack.payload[0] = request.opcode;
    previous = ack;
    request.session_id = 0xABCD0002u;
    CHECK(cdgu_ack_matches(&request, &previous) == 0);
    CHECK(cdgu_encode(&previous, words, CDTP_MAX_PACKET_WORDS,
                      &count) == 0);
    CHECK(cdgu_decode(words, count, request.session_id, &received) == -2);
    ack.session_id = request.session_id;
    CHECK(cdgu_ack_matches(&request, &ack) == 1);
    request.session_id = 0u;
    CHECK(cdgu_encode(&request, words, CDTP_MAX_PACKET_WORDS,
                      &count) == -1);
    CHECK(cdgu_ack_matches(&request, &ack) == -1);
    request.session_id = 4u;
    request.payload_length = CDGU_MAX_PAYLOAD + 1u;
    CHECK(cdgu_encode(&request, words, CDTP_MAX_PACKET_WORDS,
                      &count) == -1);
    return 0;
}

int main(void)
{
    if (roundtrip_all_sizes() || stale_session_replay())
        return 1;
    puts("Session-bound wire IDs, stale replay rejection and 253 payload sizes passed.");
    return 0;
}
