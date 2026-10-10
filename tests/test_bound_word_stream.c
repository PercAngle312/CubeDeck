#include "cubedeck/bound_stream.h"
#include "cubedeck/packet_stream.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define SIZE 65539u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

/* Simulate a packet traveling as individual 32-bit words over either side
 * of a Joybus connection. A timeout is deliberately owned by the caller. */
static int feed_packet(cdps_parser *parser, const uint32_t *words,
                       size_t count, cdtp_packet *decoded,
                       int corrupt, int truncate)
{
    size_t i, n = truncate ? count - 1u : count;
    int status = 0;
    for (i = 0u; i < n; ++i) {
        uint32_t word = words[i];
        if (corrupt && i == count - 1u)
            word ^= 1u;
        status = cdps_feed(parser, word, decoded);
        if (status != 0)
            return status;
    }
    return 0;
}

typedef struct {
    uint8_t source[SIZE];
    uint8_t target[SIZE];
    size_t offset;
    unsigned int reads;
} mock;

static int source(void *context, size_t offset,
                  uint8_t *output, size_t length)
{
    mock *m = (mock *)context;
    if (offset > SIZE || length > SIZE - offset)
        return -1;
    memcpy(output, m->source + offset, length);
    ++m->reads;
    return 0;
}

static int sink(void *context, size_t offset,
                const uint8_t *input, size_t length)
{
    mock *m = (mock *)context;
    if (offset != m->offset || offset > SIZE ||
        length > SIZE - offset)
        return -1;
    memcpy(m->target + offset, input, length);
    m->offset += length;
    return 0;
}

static int run(void)
{
    static mock m;
    cdtx_sender sender;
    cdst_receiver receiver;
    cdps_parser incoming, outgoing;
    cdtp_packet validated;
    uint32_t request[CDTP_MAX_PACKET_WORDS];
    uint32_t response[CDTP_MAX_PACKET_WORDS];
    uint32_t late_ack[CDTP_MAX_PACKET_WORDS];
    size_t n, response_n, late_n = 0u;
    uint32_t id = 0x71A93C2Du;
    unsigned int i, steps = 0u;
    for (i = 0u; i < SIZE; ++i)
        m.source[i] = (uint8_t)(i * 19u ^ (i >> 5u));
    cdps_init(&incoming);
    cdps_init(&outgoing);
    cdst_init(&receiver, sink, &m);
    CHECK(cdbs_init_sender(&sender, source, &m, SIZE,
                           cdim_crc32(m.source, SIZE)) == 0);
    while (sender.phase != CDTX_COMPLETE) {
        cdgu_message message;
        CHECK(++steps < 280u);
        CHECK(cdbs_next_frame(&sender, id, request,
                              CDTP_MAX_PACKET_WORDS, &n) == 1);
        CHECK(cdgu_decode(request, n, id, &message) == 0);

        /* Stale ACK with matching DATA sequence zero is still rejected. */
        if (message.opcode == CDCS_OP_DATA && message.sequence == 0u) {
            size_t offset = sender.offset;
            CHECK(late_n != 0u);
            CHECK(feed_packet(&outgoing, late_ack, late_n,
                              &validated, 0, 0) == 1);
            CHECK(cdbs_sender_reply(&sender, id, late_ack, late_n) == 0);
            CHECK(sender.offset == offset);
        }
        if (message.opcode == CDCS_OP_DATA && message.sequence == 3u) {
            CHECK(feed_packet(&incoming, request, n,
                              &validated, 1, 0) == -1);
            CHECK(receiver.offset == 3u * CDGU_MAX_PAYLOAD);
            CHECK(cdtx_timeout(&sender) == 0);
        }
        if (message.opcode == CDCS_OP_DATA && message.sequence == 7u) {
            CHECK(feed_packet(&incoming, request, n,
                              &validated, 0, 1) == 0);
            CHECK(incoming.count != 0u);
            cdps_init(&incoming);
            CHECK(cdtx_timeout(&sender) == 0);
        }
        CHECK(feed_packet(&incoming, request, n,
                          &validated, 0, 0) == 1);
        CHECK(cdbs_receive(&receiver, id, request, n,
                           response, CDTP_MAX_PACKET_WORDS,
                           &response_n) == 1);
        if (message.opcode == CDVS_OP_MANIFEST) {
            memcpy(late_ack, response, response_n * sizeof(response[0]));
            late_n = response_n;
        }
        CHECK(feed_packet(&outgoing, response, response_n,
                          &validated, 0, 0) == 1);

        if (message.opcode == CDCS_OP_DATA && message.sequence == 11u) {
            size_t before = receiver.offset;
            unsigned int reads = m.reads;
            CHECK(cdtx_timeout(&sender) == 0);
            CHECK(cdbs_next_frame(&sender, id, request,
                                  CDTP_MAX_PACKET_WORDS, &n) == 1);
            CHECK(m.reads == reads);
            CHECK(feed_packet(&incoming, request, n,
                              &validated, 0, 0) == 1);
            CHECK(cdbs_receive(&receiver, id, request, n,
                               response, CDTP_MAX_PACKET_WORDS,
                               &response_n) == 1);
            CHECK(receiver.offset == before);
            CHECK(feed_packet(&outgoing, response, response_n,
                              &validated, 0, 0) == 1);
        }
        CHECK(cdbs_sender_reply(&sender, id, response, response_n) == 1);
    }
    CHECK(sender.offset == SIZE && receiver.offset == SIZE);
    CHECK(receiver.verified);
    CHECK(m.offset == SIZE);
    CHECK(memcmp(m.source, m.target, SIZE) == 0);

    /* The previous session may still have a valid CRC, but not the new ID. */
    CHECK(cdbs_sender_reply(&sender, id + 1u, late_ack, late_n) == 0);
    return 0;
}

int main(void)
{
    if (run()) return 1;
    puts("Transfer-ID-bound 64 KiB+ Joybus word simulations passed.");
    return 0;
}
