#include "cubedeck/stream_sender.h"
#include "cubedeck/stream_wire.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define LENGTH 131077u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t source[LENGTH];
    uint8_t destination[LENGTH];
    size_t written;
    unsigned int reads;
    int fail_read_once;
} transfer_ctx;

static int source_read(void *opaque, size_t offset,
                       uint8_t *out, size_t len)
{
    transfer_ctx *ctx = (transfer_ctx *)opaque;
    if (ctx->fail_read_once) {
        ctx->fail_read_once = 0;
        return -1;
    }
    if (offset > LENGTH || len > LENGTH - offset)
        return -1;
    memcpy(out, ctx->source + offset, len);
    ++ctx->reads;
    return 0;
}

static int sink_write(void *opaque, size_t offset,
                      const uint8_t *in, size_t len)
{
    transfer_ctx *ctx = (transfer_ctx *)opaque;
    if (offset != ctx->written || offset > LENGTH ||
        len > LENGTH - offset)
        return -1;
    memcpy(ctx->destination + offset, in, len);
    ctx->written += len;
    return 0;
}

static int dispatch(cdst_receiver *receiver, const cdtp_packet *packet,
                    cdtp_packet *reply, int corrupt)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    if (cdtp_encode(packet, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -1;
    if (corrupt)
        words[count - 1u] ^= 0x10000u;
    return cdstw_receive_frame(receiver, words, count, reply);
}

static int test_transfer(void)
{
    static transfer_ctx context;
    cdtx_sender sender;
    cdst_receiver receiver;
    cdtp_packet packet, repeat, reply, stray;
    size_t i, steps = 0;
    unsigned int previous_reads;
    for (i = 0; i < LENGTH; ++i)
        context.source[i] = (uint8_t)((i * 19u) ^ (i >> 7u));
    cdst_init(&receiver, sink_write, &context);
    CHECK(cdtx_init(&sender, source_read, &context, LENGTH,
                    cdim_crc32(context.source, LENGTH)) == 0);
    context.fail_read_once = 1;

    while (sender.phase != CDTX_COMPLETE) {
        CHECK(++steps < 600u);
        if (context.fail_read_once && sender.phase == CDTX_DATA) {
            CHECK(cdtx_next(&sender, &packet) == -1);
            CHECK(!sender.pending_valid);
        }
        CHECK(cdtx_next(&sender, &packet) == 1);
        previous_reads = context.reads;
        CHECK(cdtx_next(&sender, &repeat) == 1);
        CHECK(memcmp(&packet, &repeat, sizeof(packet)) == 0);
        CHECK(context.reads == previous_reads);

        if (packet.opcode == CDCS_OP_DATA && packet.sequence == 5u &&
            sender.retries == 0u) {
            CHECK(dispatch(&receiver, &packet, &reply, 1) == 0);
            CHECK(reply.opcode == CDRL_OP_NACK);
            CHECK(cdtx_reply(&sender, &reply) == 0);
            CHECK(cdtx_next(&sender, &repeat) == 1);
            CHECK(memcmp(&packet, &repeat, sizeof(packet)) == 0);
        }
        CHECK(dispatch(&receiver, &packet, &reply, 0) == 1);
        if (packet.opcode == CDCS_OP_DATA &&
            packet.sequence == 10u && sender.retries == 0u) {
            size_t before = receiver.offset;
            CHECK(cdtx_timeout(&sender) == 0);
            CHECK(dispatch(&receiver, &packet, &reply, 0) == 1);
            CHECK(receiver.offset == before);
        }
        CHECK(cdrl_make_reply(CDRL_OP_ACK,
                              (uint16_t)(packet.sequence + 1u), &stray) == 0);
        CHECK(cdtx_reply(&sender, &stray) == 0);
        CHECK(cdtx_reply(&sender, &reply) == 1);
    }
    CHECK(sender.offset == LENGTH);
    CHECK(receiver.verified);
    CHECK(receiver.offset == LENGTH);
    CHECK(context.written == LENGTH);
    CHECK(memcmp(context.source, context.destination, LENGTH) == 0);
    CHECK(cdtx_next(&sender, &packet) == 0);
    return 0;
}

static int test_fail_closed(void)
{
    cdtx_sender sender;
    cdtp_packet packet, response;
    unsigned int i;
    transfer_ctx context;
    memset(&context, 0, sizeof(context));
    CHECK(cdtx_init(NULL, source_read, &context, 0u, 0u) == -1);
    CHECK(cdtx_init(&sender, NULL, &context, 0u, 0u) == -1);
    CHECK(cdtx_init(&sender, source_read, &context, CDST_MAX_BYTES + 1u, 0u) == -1);
    CHECK(cdtx_init(&sender, source_read, &context, 0u, 0u) == 0);
    CHECK(cdtx_next(&sender, &packet) == 1);
    for (i = 0; i < CDRL_MAX_RETRIES; ++i)
        CHECK(cdtx_timeout(&sender) == 0);
    CHECK(cdtx_timeout(&sender) == -1);
    CHECK(sender.phase == CDTX_FAILED);
    CHECK(cdtx_next(&sender, &response) == -1);
    return 0;
}

int main(void)
{
    if (test_transfer() || test_fail_closed())
        return 1;
    puts("Cached streaming sender: 128 KiB+ manifest-to-FINISH transfer passed.");
    return 0;
}
