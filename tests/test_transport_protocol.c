#include "cubedeck/transport_protocol.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void test_empty_packet(void)
{
    cdtp_packet input;
    cdtp_packet output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 0;

    memset(&input, 0, sizeof(input));
    input.opcode = 1u;
    input.sequence = 0x1234u;
    input.payload_length = 0u;

    CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    CHECK(count == 4u);
    CHECK(words[0] == CDTP_MAGIC);
    CHECK(words[1] == 0x01011234u);
    CHECK(cdtp_decode(words, count, &output) == CDTP_OK);
    CHECK(output.opcode == input.opcode);
    CHECK(output.sequence == input.sequence);
    CHECK(output.payload_length == 0u);
}

static void test_payload_round_trip(void)
{
    static const uint8_t sample[] = {0x00u, 0x11u, 0x22u, 0x33u, 0xFEu, 0xDCu, 0xBAu};
    cdtp_packet input;
    cdtp_packet output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 0;

    memset(&input, 0, sizeof(input));
    input.opcode = 0x42u;
    input.sequence = 0xBEEFu;
    input.payload_length = (uint16_t)sizeof(sample);
    memcpy(input.payload, sample, sizeof(sample));

    CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    CHECK(count == 6u);
    CHECK(words[3] == 0x33221100u);
    CHECK(words[4] == 0x00BADCFEu);
    CHECK(cdtp_decode(words, count, &output) == CDTP_OK);
    CHECK(output.opcode == input.opcode);
    CHECK(output.sequence == input.sequence);
    CHECK(output.payload_length == input.payload_length);
    CHECK(memcmp(output.payload, sample, sizeof(sample)) == 0);
}

static void test_max_payload(void)
{
    cdtp_packet input;
    cdtp_packet output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 0;
    size_t i;

    memset(&input, 0, sizeof(input));
    input.opcode = 3u;
    input.sequence = 0xFFFFu;
    input.payload_length = CDTP_MAX_PAYLOAD;
    for (i = 0; i < CDTP_MAX_PAYLOAD; ++i)
        input.payload[i] = (uint8_t)(i ^ (i >> 2));

    CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    CHECK(count == CDTP_MAX_PACKET_WORDS);
    CHECK(cdtp_decode(words, count, &output) == CDTP_OK);
    CHECK(output.payload_length == CDTP_MAX_PAYLOAD);
    CHECK(memcmp(output.payload, input.payload, CDTP_MAX_PAYLOAD) == 0);
}

static void test_rejects_corruption_and_bad_shape(void)
{
    cdtp_packet input;
    cdtp_packet output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 0;

    memset(&input, 0, sizeof(input));
    input.opcode = 2u;
    input.sequence = 7u;
    input.payload_length = 4u;
    input.payload[0] = 1u;
    input.payload[1] = 2u;
    input.payload[2] = 3u;
    input.payload[3] = 4u;
    CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);

    words[3] ^= 1u;
    CHECK(cdtp_decode(words, count, &output) == CDTP_ERR_CRC);
    words[3] ^= 1u;
    CHECK(cdtp_decode(words, count - 1u, &output) == CDTP_ERR_FORMAT);
    CHECK(cdtp_decode(words, count + 1u, &output) == CDTP_ERR_FORMAT);

    words[0] = 0u;
    CHECK(cdtp_decode(words, count, &output) == CDTP_ERR_FORMAT);
}

static void test_argument_and_capacity_errors(void)
{
    cdtp_packet input;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 99u;

    memset(&input, 0, sizeof(input));
    input.payload_length = 1u;
    input.payload[0] = 0xAAu;
    CHECK(cdtp_encode(&input, words, 0u, &count) == CDTP_ERR_CAPACITY);
    CHECK(count == 0u);
    CHECK(cdtp_encode(NULL, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_ERR_ARGUMENT);
    CHECK(cdtp_decode(NULL, 0u, &input) == CDTP_ERR_ARGUMENT);
}

int main(void)
{
    test_empty_packet();
    test_payload_round_trip();
    test_max_payload();
    test_rejects_corruption_and_bad_shape();
    test_argument_and_capacity_errors();

    if (failures != 0) {
        fprintf(stderr, "%d protocol test(s) failed\n", failures);
        return 1;
    }

    puts("All CubeDeck transport protocol tests passed.");
    return 0;
}
