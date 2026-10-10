#include "cubedeck/stream_wire.h"
#include "cubedeck/image_integrity.h"
#include <stdio.h>
#include <string.h>

#define TEST_BYTES 131077u
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    uint8_t bytes[TEST_BYTES];
    size_t written;
    int reject_once;
} ram_sink;

static int sink_write(void *context, size_t offset,
                      const uint8_t *payload, size_t length)
{
    ram_sink *sink = (ram_sink *)context;
    if (sink->reject_once) {
        sink->reject_once = 0;
        return -1;
    }
    if (offset != sink->written || offset > TEST_BYTES ||
        length > TEST_BYTES - offset)
        return -1;
    memcpy(sink->bytes + offset, payload, length);
    sink->written += length;
    return 0;
}

static int wire(cdst_receiver *receiver, const cdtp_packet *input,
                cdtp_packet *response, int damaged)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    if (cdtp_encode(input, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -1;
    if (damaged)
        words[count - 1u] ^= 0x00010000u;
    return cdstw_receive_frame(receiver, words, count, response);
}

static void put32(uint8_t *ptr, uint32_t value)
{
    unsigned int i;
    for (i = 0; i < 4u; ++i)
        ptr[i] = (uint8_t)(value >> (i * 8u));
}

static int test_128k_transfer(void)
{
    static uint8_t source[TEST_BYTES];
    static ram_sink sink;
    cdst_receiver receiver;
    cdtp_packet request, response;
    size_t offset = 0u, i;
    uint16_t sequence = 0u;
    for (i = 0; i < TEST_BYTES; ++i)
        source[i] = (uint8_t)(i * 37u + (i >> 5u));
    memset(&sink, 0, sizeof(sink));
    cdst_init(&receiver, sink_write, &sink);

    memset(&request, 0, sizeof(request));
    request.opcode = CDVS_OP_MANIFEST;
    request.payload_length = CDVS_MANIFEST_BYTES;
    put32(request.payload, TEST_BYTES);
    put32(request.payload + 4u, cdim_crc32(source, TEST_BYTES));
    CHECK(wire(&receiver, &request, &response, 1) == 0);
    CHECK(response.opcode == CDRL_OP_NACK && !receiver.has_manifest);
    CHECK(wire(&receiver, &request, &response, 0) == 1);
    CHECK(response.opcode == CDRL_OP_ACK);
    CHECK(wire(&receiver, &request, &response, 0) == 1);

    while (offset < TEST_BYTES) {
        size_t length = TEST_BYTES - offset;
        if (length > CDTP_MAX_PAYLOAD)
            length = CDTP_MAX_PAYLOAD;
        memset(&request, 0, sizeof(request));
        request.opcode = CDCS_OP_DATA;
        request.sequence = sequence;
        request.payload_length = (uint16_t)length;
        memcpy(request.payload, source + offset, length);

        if (sequence == 40u) {
            CHECK(wire(&receiver, &request, &response, 1) == 0);
            CHECK(response.opcode == CDRL_OP_NACK);
            CHECK(response.sequence == sequence);
            CHECK(receiver.offset == offset);
        }
        if (sequence == 64u) {
            sink.reject_once = 1;
            CHECK(wire(&receiver, &request, &response, 0) == 0);
            CHECK(response.opcode == CDRL_OP_NACK);
            CHECK(receiver.offset == offset);
        }
        CHECK(wire(&receiver, &request, &response, 0) == 1);
        CHECK(response.opcode == CDRL_OP_ACK);
        CHECK(response.sequence == sequence);
        if (sequence == 100u || offset + length == TEST_BYTES) {
            /* Retransmit identical packet after dropped ACK. */
            CHECK(wire(&receiver, &request, &response, 0) == 1);
            CHECK(receiver.offset == offset + length);
        }
        offset += length;
        ++sequence;
    }
    CHECK(sequence > 512u);
    CHECK(sink.written == TEST_BYTES);
    CHECK(memcmp(source, sink.bytes, TEST_BYTES) == 0);
    CHECK(!receiver.verified);
    memset(&request, 0, sizeof(request));
    request.opcode = CDVS_OP_FINISH;
    request.sequence = sequence;
    CHECK(wire(&receiver, &request, &response, 0) == 1);
    CHECK(response.opcode == CDRL_OP_ACK && receiver.verified);
    CHECK(wire(&receiver, &request, &response, 0) == 1);
    return 0;
}

int main(void)
{
    if (test_128k_transfer())
        return 1;
    puts("128 KiB+ CRC-framed streaming exchange and retry scenarios passed.");
    return 0;
}
