#include "cubedeck/stream_receiver.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define IMAGE_LENGTH 32771u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t bytes[IMAGE_LENGTH];
    size_t written;
    int reject_once;
} memory_sink;

static int store(void *context, size_t offset,
                 const uint8_t *bytes, size_t length)
{
    memory_sink *s = (memory_sink *)context;
    if (s->reject_once) {
        s->reject_once = 0;
        return -1;
    }
    if (offset != s->written || length > IMAGE_LENGTH - offset)
        return -1;
    memcpy(s->bytes + offset, bytes, length);
    s->written += length;
    return 0;
}

static void manifest(cdtp_packet *p, uint32_t len, uint32_t crc)
{
    unsigned int i;
    memset(p, 0, sizeof(*p));
    p->opcode = CDVS_OP_MANIFEST;
    p->payload_length = CDVS_MANIFEST_BYTES;
    for (i = 0u; i < 4u; ++i) {
        p->payload[i] = (uint8_t)(len >> (8u * i));
        p->payload[4u + i] = (uint8_t)(crc >> (8u * i));
    }
}

static int test_large_stream(void)
{
    static uint8_t source[IMAGE_LENGTH];
    static memory_sink sink;
    cdst_receiver receiver;
    cdtp_packet p;
    size_t i, offset = 0u;
    uint16_t seq = 0u;
    for (i = 0u; i < IMAGE_LENGTH; ++i)
        source[i] = (uint8_t)(i * 31u + (i >> 4u));
    memset(&sink, 0, sizeof(sink));
    cdst_init(&receiver, store, &sink);
    memset(&p, 0, sizeof(p));
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_STATE);
    manifest(&p, IMAGE_LENGTH, cdim_crc32(source, IMAGE_LENGTH));
    CHECK(cdst_receive(&receiver, &p) == CDST_ACCEPTED);
    CHECK(cdst_receive(&receiver, &p) == CDST_DUPLICATE);
    while (offset < IMAGE_LENGTH) {
        size_t len = IMAGE_LENGTH - offset;
        uint32_t before_crc = receiver.crc_state;
        if (len > CDTP_MAX_PAYLOAD)
            len = CDTP_MAX_PAYLOAD;
        memset(&p, 0, sizeof(p));
        p.opcode = CDCS_OP_DATA;
        p.sequence = seq;
        p.payload_length = (uint16_t)len;
        memcpy(p.payload, source + offset, len);
        if (seq == 5u) {
            sink.reject_once = 1;
            CHECK(cdst_receive(&receiver, &p) == CDST_ERR_SINK);
            CHECK(receiver.crc_state == before_crc);
            CHECK(receiver.offset == offset);
            CHECK(receiver.next_sequence == seq);
        }
        CHECK(cdst_receive(&receiver, &p) == CDST_ACCEPTED);
        CHECK(cdst_receive(&receiver, &p) == CDST_DUPLICATE);
        offset += len;
        ++seq;
    }
    CHECK(seq > 100u);
    CHECK(receiver.offset == IMAGE_LENGTH);
    CHECK(sink.written == IMAGE_LENGTH);
    CHECK(memcmp(sink.bytes, source, IMAGE_LENGTH) == 0);
    memset(&p, 0, sizeof(p));
    p.opcode = CDVS_OP_FINISH;
    p.sequence = seq;
    CHECK(cdst_receive(&receiver, &p) == CDST_VERIFIED);
    CHECK(receiver.verified == 1);
    CHECK(cdst_receive(&receiver, &p) == CDST_VERIFIED);
    p.opcode = CDCS_OP_DATA;
    p.payload_length = 1u;
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_STATE);
    return 0;
}

static int test_fail_closed(void)
{
    memory_sink sink;
    cdst_receiver receiver;
    cdtp_packet p;
    memset(&sink, 0, sizeof(sink));
    cdst_init(&receiver, store, &sink);
    CHECK(cdst_receive(NULL, &p) == CDST_ERR_ARGUMENT);
    cdst_init(&receiver, NULL, NULL);
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_ARGUMENT);
    cdst_init(&receiver, store, &sink);
    manifest(&p, CDST_MAX_BYTES + 1u, 0u);
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_CAPACITY);
    manifest(&p, 2u, 0x12345678u);
    CHECK(cdst_receive(&receiver, &p) == CDST_ACCEPTED);
    p.opcode = CDCS_OP_DATA;
    p.payload_length = 3u;
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_CAPACITY);
    p.payload_length = 2u;
    CHECK(cdst_receive(&receiver, &p) == CDST_ACCEPTED);
    p.payload[0] ^= 1u;
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_SEQUENCE);
    memset(&p, 0, sizeof(p));
    p.opcode = CDVS_OP_FINISH;
    p.sequence = 1u;
    CHECK(cdst_receive(&receiver, &p) == CDST_ERR_INTEGRITY);
    CHECK(!receiver.verified);
    return 0;
}

int main(void)
{
    if (test_large_stream() || test_fail_closed())
        return 1;
    puts("RAM-light 32 KiB stream receiver and failure cases passed.");
    return 0;
}
