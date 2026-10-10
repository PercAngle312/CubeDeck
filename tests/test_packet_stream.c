#include "cubedeck/packet_stream.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\\n", __LINE__, #x); return 1; } } while (0)

static int feed_packet(cdps_parser *parser, const cdtp_packet *input,
                       cdtp_packet *output, int corrupt)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count, i;
    int result = 0;
    CHECK(cdtp_encode(input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    if (corrupt)
        words[count - 1u] ^= 1u;
    for (i = 0u; i < count; ++i) {
        result = cdps_feed(parser, words[i], output);
        if (i + 1u < count)
            CHECK(result == 0);
    }
    return result;
}

static int test_all_sizes_and_recovery(void)
{
    cdps_parser parser;
    cdtp_packet input, output;
    size_t length, i;
    cdps_init(&parser);
    CHECK(cdps_feed(NULL, 0u, &output) == -1);
    CHECK(cdps_feed(&parser, 0u, NULL) == -1);
    CHECK(cdps_feed(&parser, 0x12345678u, &output) == 0);
    memset(&input, 0, sizeof(input));
    for (length = 0u; length <= CDTP_MAX_PAYLOAD; ++length) {
        input.opcode = (uint8_t)(length + 4u);
        input.sequence = (uint16_t)(length + 51u);
        input.payload_length = (uint16_t)length;
        for (i = 0u; i < length; ++i)
            input.payload[i] = (uint8_t)(i * 3u + length);
        /* The magic word inside payload data must not restart the parser. */
        if (length >= 4u && (length % 5u) == 0u) {
            input.payload[0] = 0x4Bu;
            input.payload[1] = 0x42u;
            input.payload[2] = 0x44u;
            input.payload[3] = 0x43u;
        }
        CHECK(feed_packet(&parser, &input, &output, 0) == 1);
        CHECK(output.opcode == input.opcode);
        CHECK(output.sequence == input.sequence);
        CHECK(output.payload_length == length);
        CHECK(memcmp(output.payload, input.payload, length) == 0);
        if (length % 17u == 0u) {
            CHECK(feed_packet(&parser, &input, &output, 1) == -1);
            CHECK(feed_packet(&parser, &input, &output, 0) == 1);
        }
        CHECK(parser.count == 0u && parser.expected == 0u);
    }
    CHECK(parser.accepted > 257u);
    CHECK(parser.rejected > 0u);
    return 0;
}

static int test_bad_headers(void)
{
    cdps_parser parser;
    cdtp_packet output, input;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    cdps_init(&parser);
    CHECK(cdps_feed(&parser, CDTP_MAGIC, &output) == 0);
    CHECK(cdps_feed(&parser, 0xFF000000u, &output) == -1);
    CHECK(parser.count == 0u);
    CHECK(cdps_feed(&parser, CDTP_MAGIC, &output) == 0);
    CHECK(cdps_feed(&parser, 0x01000000u, &output) == 0);
    CHECK(cdps_feed(&parser, CDTP_MAX_PAYLOAD + 1u, &output) == -1);
    CHECK(parser.count == 0u && parser.expected == 0u);
    memset(&input, 0, sizeof(input));
    input.payload_length = 4u;
    CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    CHECK(cdps_feed(&parser, words[0], &output) == 0);
    CHECK(cdps_feed(&parser, words[1], &output) == 0);
    CHECK(cdps_feed(&parser, words[2], &output) == 0);
    CHECK(cdps_feed(&parser, words[3] ^ 0x10u, &output) == 0);
    CHECK(cdps_feed(&parser, words[4], &output) == -1);
    CHECK(feed_packet(&parser, &input, &output, 0) == 1);
    return 0;
}

int main(void)
{
    if (test_all_sizes_and_recovery() || test_bad_headers())
        return 1;
    puts("Incremental word-stream parser tests passed.");
    return 0;
}
