#include "cubedeck/packet_stream.h"
#include "cubedeck/stream_sender.h"
#include "cubedeck/stream_wire.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define IMAGE_BYTES 65539u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t source[IMAGE_BYTES];
    uint8_t destination[IMAGE_BYTES];
    size_t written;
    unsigned int reads;
    unsigned int writes;
} ram_link;

static int source_read(void *opaque, size_t offset, uint8_t *out, size_t length)
{
    ram_link *link = (ram_link *)opaque;
    if (offset > IMAGE_BYTES || length > IMAGE_BYTES - offset)
        return -1;
    memcpy(out, link->source + offset, length);
    ++link->reads;
    return 0;
}

static int sink_write(void *opaque, size_t offset,
                      const uint8_t *bytes, size_t length)
{
    ram_link *link = (ram_link *)opaque;
    if (offset != link->written || offset > IMAGE_BYTES ||
        length > IMAGE_BYTES - offset)
        return -1;
    memcpy(link->destination + offset, bytes, length);
    link->written += length;
    ++link->writes;
    return 0;
}

/* Return 1 iff an entire valid packet arrives, 0 when truncated,
 * -1 if its CRC fails. Simulates one cable word at a time.
 */
static int send_words(cdps_parser *parser, const cdtp_packet *frame,
                      cdtp_packet *delivered, int fault)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count, i, limit;
    int result = 0;
    if (cdtp_encode(frame, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -2;
    limit = fault == 2 ? count - 1u : count;
    if (fault == 1)
        words[count - 1u] ^= 1u;
    for (i = 0u; i < limit; ++i) {
        result = cdps_feed(parser, words[i], delivered);
        if (i + 1u < count && result != 0)
            return -2;
    }
    return result;
}

static int full_transfer(void)
{
    static ram_link ram;
    cdtx_sender sender;
    cdst_receiver receiver;
    cdps_parser recv_parser, reply_parser;
    cdtp_packet outgoing, accepted, reply, response;
    unsigned int transactions = 0u;
    size_t i;
    memset(&ram, 0, sizeof(ram));
    for (i = 0u; i < IMAGE_BYTES; ++i)
        ram.source[i] = (uint8_t)(i * 17u ^ (i >> 6u));
    cdps_init(&recv_parser);
    cdps_init(&reply_parser);
    cdst_init(&receiver, sink_write, &ram);
    CHECK(cdtx_init(&sender, source_read, &ram, IMAGE_BYTES,
                    cdim_crc32(ram.source, IMAGE_BYTES)) == 0);
    while (sender.phase != CDTX_COMPLETE) {
        unsigned int phase;
        CHECK(++transactions < 310u);
        CHECK(cdtx_next(&sender, &outgoing) == 1);
        phase = (unsigned int)outgoing.sequence;

        if (outgoing.opcode == CDCS_OP_DATA && phase == 3u) {
            size_t before = receiver.offset;
            CHECK(send_words(&recv_parser, &outgoing, &accepted, 1) == -1);
            CHECK(receiver.offset == before);
            CHECK(cdtx_timeout(&sender) == 0);
            CHECK(cdtx_next(&sender, &accepted) == 1);
            CHECK(memcmp(&accepted, &outgoing, sizeof(outgoing)) == 0);
        }
        if (outgoing.opcode == CDCS_OP_DATA && phase == 7u) {
            CHECK(send_words(&recv_parser, &outgoing, &accepted, 2) == 0);
            CHECK(recv_parser.count != 0u);
            cdps_init(&recv_parser); /* Caller-owned timeout recovery. */
            CHECK(cdtx_timeout(&sender) == 0);
        }

        CHECK(send_words(&recv_parser, &outgoing, &accepted, 0) == 1);
        CHECK(cdstw_receive_decoded(&receiver, &accepted, &reply) == 1);
        CHECK(reply.opcode == CDRL_OP_ACK);
        CHECK(send_words(&reply_parser, &reply, &response, 0) == 1);

        if (outgoing.opcode == CDCS_OP_DATA && phase == 11u) {
            size_t before = receiver.offset;
            unsigned int reads = ram.reads;
            CHECK(cdtx_timeout(&sender) == 0);
            CHECK(cdtx_next(&sender, &accepted) == 1);
            CHECK(memcmp(&accepted, &outgoing, sizeof(outgoing)) == 0);
            CHECK(ram.reads == reads);
            CHECK(send_words(&recv_parser, &accepted, &accepted, 0) == 1);
            CHECK(cdstw_receive_decoded(&receiver, &accepted, &reply) == 1);
            CHECK(receiver.offset == before);
            CHECK(send_words(&reply_parser, &reply, &response, 0) == 1);
        }
        CHECK(cdtx_reply(&sender, &response) == 1);
    }
    CHECK(receiver.verified);
    CHECK(receiver.offset == IMAGE_BYTES);
    CHECK(sender.offset == IMAGE_BYTES);
    CHECK(ram.written == IMAGE_BYTES);
    CHECK(memcmp(ram.source, ram.destination, IMAGE_BYTES) == 0);
    CHECK(recv_parser.accepted >= transactions);
    CHECK(reply_parser.accepted >= transactions);
    return 0;
}

int main(void)
{
    if (full_transfer())
        return 1;
    puts("64 KiB+ word-by-word streaming with CRC, timeout and ACK passed.");
    return 0;
}
