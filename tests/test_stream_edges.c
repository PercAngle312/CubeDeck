#include "cubedeck/stream_sender.h"
#include "cubedeck/stream_wire.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    unsigned int reads;
    unsigned int writes;
} counters;

static int reader(void *arg, size_t offset, uint8_t *dest, size_t length)
{
    counters *c = (counters *)arg;
    (void)offset;
    (void)dest;
    (void)length;
    ++c->reads;
    return -1;
}

static int writer(void *arg, size_t offset, const uint8_t *data, size_t length)
{
    counters *c = (counters *)arg;
    (void)offset;
    (void)data;
    (void)length;
    ++c->writes;
    return -1;
}

static int deliver(cdst_receiver *receiver, const cdtp_packet *frame,
                   cdtp_packet *reply)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    CHECK(cdtp_encode(frame, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    return cdstw_receive_frame(receiver, words, count, reply);
}

static int zero_length_roundtrip(void)
{
    counters calls = {0u, 0u};
    cdtx_sender sender;
    cdst_receiver receiver;
    cdtp_packet frame, reply, stale;
    CHECK(cdim_crc32(NULL, 0u) == 0u);
    CHECK(cdtx_init(&sender, reader, &calls, 0u, 0u) == 0);
    cdst_init(&receiver, writer, &calls);
    CHECK(cdtx_next(&sender, &frame) == 1);
    CHECK(frame.opcode == CDVS_OP_MANIFEST);
    CHECK(frame.payload_length == CDVS_MANIFEST_BYTES);
    CHECK(deliver(&receiver, &frame, &reply) == 1);
    stale = reply;
    CHECK(cdtx_reply(&sender, &reply) == 1);
    CHECK(sender.phase == CDTX_FINISH);
    CHECK(cdtx_next(&sender, &frame) == 1);
    CHECK(frame.opcode == CDVS_OP_FINISH && frame.sequence == 0u);
    /* A delayed sequence-zero MANIFEST ACK cannot complete FINISH. */
    CHECK(cdtx_reply(&sender, &stale) == 0);
    CHECK(sender.phase == CDTX_FINISH);
    CHECK(deliver(&receiver, &frame, &reply) == 1);
    CHECK(receiver.verified == 1);
    CHECK(cdtx_reply(&sender, &reply) == 1);
    CHECK(sender.phase == CDTX_COMPLETE);
    CHECK(cdtx_next(&sender, &frame) == 0);
    CHECK(calls.reads == 0u && calls.writes == 0u);
    return 0;
}

static int rejected_source_read(void)
{
    counters calls = {0u, 0u};
    cdtx_sender sender;
    cdst_receiver receiver;
    cdtp_packet frame, reply;
    uint8_t single = 9u;
    CHECK(cdtx_init(&sender, reader, &calls, 1u,
                    cdim_crc32(&single, 1u)) == 0);
    cdst_init(&receiver, writer, &calls);
    CHECK(cdtx_next(&sender, &frame) == 1);
    CHECK(deliver(&receiver, &frame, &reply) == 1);
    CHECK(cdtx_reply(&sender, &reply) == 1);
    CHECK(cdtx_next(&sender, &frame) == -1);
    CHECK(cdtx_next(&sender, &frame) == -1);
    CHECK(sender.offset == 0u && sender.next_sequence == 0u);
    CHECK(!sender.pending_valid && receiver.offset == 0u);
    CHECK(calls.reads == 2u && calls.writes == 0u);
    return 0;
}

int main(void)
{
    if (zero_length_roundtrip() || rejected_source_read())
        return 1;
    puts("Empty sessions, stale ACK isolation and source-failure checks passed.");
    return 0;
}
