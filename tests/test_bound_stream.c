#include "cubedeck/bound_stream.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define SIZE 1013u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t source[SIZE];
    uint8_t target[SIZE];
    size_t written;
    unsigned int reads;
    unsigned int writes;
} state;

static int read_source(void *context, size_t offset,
                       uint8_t *out, size_t length)
{
    state *s = (state *)context;
    if (offset > SIZE || length > SIZE - offset)
        return -1;
    memcpy(out, s->source + offset, length);
    ++s->reads;
    return 0;
}

static int write_target(void *context, size_t offset,
                        const uint8_t *in, size_t length)
{
    state *s = (state *)context;
    if (offset != s->written || offset > SIZE || length > SIZE - offset)
        return -1;
    memcpy(s->target + offset, in, length);
    s->written += length;
    ++s->writes;
    return 0;
}

static int run(void)
{
    static state s;
    cdtx_sender first, second;
    cdst_receiver first_rx, second_rx;
    uint32_t request[CDTP_MAX_PACKET_WORDS];
    uint32_t answer[CDTP_MAX_PACKET_WORDS];
    uint32_t old_ack[CDTP_MAX_PACKET_WORDS];
    uint32_t old_request[CDTP_MAX_PACKET_WORDS];
    size_t n, reply_n, old_n = 0u, old_ack_n = 0u;
    unsigned int i;
    uint32_t first_id = 0x13572468u, second_id = 0x24681357u;
    for (i = 0u; i < SIZE; ++i)
        s.source[i] = (uint8_t)(i * 17u ^ (i >> 3u));
    cdst_init(&first_rx, write_target, &s);
    CHECK(cdbs_init_sender(&first, read_source, &s, SIZE,
                           cdim_crc32(s.source, SIZE)) == 0);
    CHECK(first.chunk_capacity == CDGU_MAX_PAYLOAD);
    while (first.phase != CDTX_COMPLETE) {
        cdgu_message msg;
        CHECK(cdbs_next_frame(&first, first_id, request,
                              CDTP_MAX_PACKET_WORDS, &n) == 1);
        CHECK(cdgu_decode(request, n, first_id, &msg) == 0);
        CHECK(msg.payload_length <= CDGU_MAX_PAYLOAD);
        if (msg.opcode == CDCS_OP_DATA) {
            CHECK(msg.payload_length <= 252u);
            if (msg.sequence == 0u) {
                memcpy(old_request, request, n * sizeof(request[0]));
                old_n = n;
            }
        }
        CHECK(cdbs_receive(&first_rx, first_id, request, n, answer,
                           CDTP_MAX_PACKET_WORDS, &reply_n) == 1);
        if (msg.opcode == CDCS_OP_DATA && msg.sequence == 0u) {
            memcpy(old_ack, answer, reply_n * sizeof(answer[0]));
            old_ack_n = reply_n;
        }
        CHECK(cdbs_sender_reply(&first, first_id, answer, reply_n) == 1);
    }
    CHECK(s.written == SIZE && first_rx.verified);
    CHECK(memcmp(s.source, s.target, SIZE) == 0);
    CHECK(s.writes == 5u); /* 252 + 252 + 252 + 252 + 5 */
    CHECK(old_n > 0u && old_ack_n > 0u);

    /* New receiver reuses DATA sequence zero, but NOT the transfer ID. */
    s.written = 0u;
    s.writes = 0u;
    memset(s.target, 0, sizeof(s.target));
    cdst_init(&second_rx, write_target, &s);
    CHECK(cdbs_init_sender(&second, read_source, &s, SIZE,
                           cdim_crc32(s.source, SIZE)) == 0);
    CHECK(cdbs_next_frame(&second, second_id, request,
                          CDTP_MAX_PACKET_WORDS, &n) == 1);
    CHECK(cdbs_receive(&second_rx, second_id, old_request, old_n,
                       answer, CDTP_MAX_PACKET_WORDS, &reply_n) == 0);
    CHECK(reply_n == 0u && second_rx.offset == 0u && !second_rx.has_manifest);
    CHECK(cdbs_sender_reply(&second, second_id, old_ack, old_ack_n) == 0);
    CHECK(second.phase == CDTX_MANIFEST);
    CHECK(cdbs_receive(&second_rx, second_id, request, n, answer,
                       CDTP_MAX_PACKET_WORDS, &reply_n) == 1);
    CHECK(cdbs_sender_reply(&second, second_id, answer, reply_n) == 1);
    CHECK(cdbs_next_frame(&second, second_id, request,
                          CDTP_MAX_PACKET_WORDS, &n) == 1);
    CHECK(cdbs_sender_reply(&second, second_id, old_ack, old_ack_n) == 0);
    CHECK(second.offset == 0u && second.phase == CDTX_DATA);
    /* Bad CRC is ignored; the sender can retry on timeout. */
    request[n - 1u] ^= 1u;
    CHECK(cdbs_receive(&second_rx, second_id, request, n, answer,
                       CDTP_MAX_PACKET_WORDS, &reply_n) == 0);
    CHECK(reply_n == 0u && second_rx.offset == 0u);
    request[n - 1u] ^= 1u;

    while (second.phase != CDTX_COMPLETE) {
        CHECK(cdbs_next_frame(&second, second_id, request,
                              CDTP_MAX_PACKET_WORDS, &n) == 1);
        CHECK(cdbs_receive(&second_rx, second_id, request, n, answer,
                           CDTP_MAX_PACKET_WORDS, &reply_n) == 1);
        CHECK(cdbs_sender_reply(&second, second_id, answer, reply_n) == 1);
    }
    CHECK(second_rx.verified && s.written == SIZE && s.writes == 5u);
    CHECK(memcmp(s.source, s.target, SIZE) == 0);
    CHECK(cdbs_init_sender(&second, read_source, &s, CDBS_MAX_BYTES + 1u,
                           0u) == -1);
    return 0;
}

int main(void)
{
    if (run()) return 1;
    puts("Session-bound 252-byte streaming and old-session replay isolation passed.");
    return 0;
}
