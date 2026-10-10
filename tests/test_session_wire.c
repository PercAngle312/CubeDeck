#include "cubedeck/session_wire.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); return 1; \
} } while (0)

static int exchange(cdvs_receiver *receiver, const cdtp_packet *request,
                    cdtp_packet *reply, int corrupt)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    if (cdtp_encode(request, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -1;
    if (corrupt)
        words[count - 1u] ^= 1u; /* Malformed CRC: receiver must not mutate. */
    return cdsw_receive_frame(receiver, words, count, reply);
}

static int test_verified_wire_session(void)
{
    uint8_t source[1029];
    cdim_manifest expected;
    cdvs_receiver receiver;
    cdtp_packet request, reply;
    size_t i;
    for (i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)(i * 11u + (i >> 3));
    expected.expected_bytes = sizeof(source);
    expected.expected_crc32 = cdim_crc32(source, sizeof(source));
    cdvs_init(&receiver);

    /* DATA before manifest must not mutate state. */
    memset(&request, 0, sizeof(request));
    request.opcode = CDCS_OP_DATA;
    request.payload_length = 1u;
    CHECK(exchange(&receiver, &request, &reply, 0) == 0);
    CHECK(reply.opcode == CDRL_OP_NACK && receiver.chunks.length == 0u);

    CHECK(cdvs_make_manifest(expected.expected_bytes, expected.expected_crc32,
                             &request) == 0);
    CHECK(exchange(&receiver, &request, &reply, 1) == 0);
    CHECK(reply.opcode == CDRL_OP_NACK && !receiver.has_manifest);
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);
    CHECK(reply.opcode == CDRL_OP_ACK && reply.sequence == 0u);
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);

    for (i = 0; i < 5u; ++i) {
        size_t offset = i * CDTP_MAX_PAYLOAD;
        size_t length = sizeof(source) - offset;
        if (length > CDTP_MAX_PAYLOAD)
            length = CDTP_MAX_PAYLOAD;
        memset(&request, 0, sizeof(request));
        request.opcode = CDCS_OP_DATA;
        request.sequence = (uint16_t)i;
        request.payload_length = (uint16_t)length;
        memcpy(request.payload, source + offset, length);
        if (i == 2u) {
            size_t previous = receiver.chunks.length;
            CHECK(exchange(&receiver, &request, &reply, 1) == 0);
            CHECK(receiver.chunks.length == previous);
            CHECK(reply.opcode == CDRL_OP_NACK &&
                  reply.sequence == (uint16_t)i);
        }
        CHECK(exchange(&receiver, &request, &reply, 0) == 1);
        CHECK(reply.opcode == CDRL_OP_ACK &&
              reply.sequence == (uint16_t)i);
        if (i == 4u) {
            size_t before = receiver.chunks.length;
            /* Simulate lost ACK; duplicate final packet must not append. */
            CHECK(exchange(&receiver, &request, &reply, 0) == 1);
            CHECK(receiver.chunks.length == before);
            CHECK(reply.opcode == CDRL_OP_ACK);
        }
    }
    CHECK(receiver.chunks.length == sizeof(source));
    CHECK(memcmp(receiver.chunks.data, source, sizeof(source)) == 0);
    CHECK(receiver.verified == 0);

    CHECK(cdvs_make_finish(4u, &request) == 0);
    CHECK(exchange(&receiver, &request, &reply, 0) == 0);
    CHECK(reply.opcode == CDRL_OP_NACK && reply.sequence == 5u);
    CHECK(cdvs_make_finish(5u, &request) == 0);
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);
    CHECK(reply.opcode == CDRL_OP_ACK && reply.sequence == 5u);
    CHECK(receiver.verified == 1);
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);
    return 0;
}

static int test_checksum_failure_and_invalid_args(void)
{
    cdvs_receiver receiver;
    cdtp_packet request, reply;
    cdvs_init(&receiver);
    CHECK(cdsw_receive_frame(NULL, NULL, 0u, &reply) == -1);
    CHECK(cdvs_make_manifest(2u, 0x12345678u, &request) == 0);
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);
    memset(&request, 0, sizeof(request));
    request.opcode = CDCS_OP_DATA;
    request.payload_length = 2u;
    request.payload[0] = 1u;
    request.payload[1] = 2u;
    CHECK(exchange(&receiver, &request, &reply, 0) == 1);
    CHECK(cdvs_make_finish(1u, &request) == 0);
    CHECK(exchange(&receiver, &request, &reply, 0) == 0);
    CHECK(reply.opcode == CDRL_OP_NACK);
    CHECK(receiver.verified == 0);
    return 0;
}

int main(void)
{
    if (test_verified_wire_session() ||
        test_checksum_failure_and_invalid_args())
        return 1;
    puts("Verified wire session tests passed.");
    return 0;
}
