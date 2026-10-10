#include "cubedeck/transport_protocol.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

/* Exercise every supported payload size and corrupt every encoded word.
 * Keep this repeatable: failures can be reproduced from the logged length
 * and word index without relying on a random seed.
 */
static int test_packet_matrix(void)
{
    cdtp_packet input, output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t length, count, index, i;
    for (length = 0; length <= CDTP_MAX_PAYLOAD; ++length) {
        memset(&input, 0, sizeof(input));
        input.opcode = (uint8_t)(length * 13u + 7u);
        input.sequence = (uint16_t)(length * 251u + 0x1234u);
        input.payload_length = (uint16_t)length;
        for (i = 0; i < length; ++i)
            input.payload[i] = (uint8_t)(i * 37u + length);
        CHECK(cdtp_encode(&input, words, CDTP_MAX_PACKET_WORDS,
                          &count) == CDTP_OK);
        CHECK(count == CDTP_HEADER_WORDS + ((length + 3u) / 4u) + 1u);
        memset(&output, 0xA5, sizeof(output));
        CHECK(cdtp_decode(words, count, &output) == CDTP_OK);
        CHECK(output.opcode == input.opcode);
        CHECK(output.sequence == input.sequence);
        CHECK(output.payload_length == input.payload_length);
        CHECK(memcmp(input.payload, output.payload, length) == 0);

        /* One-bit corruption of any header, data, or CRC word must fail. */
        for (index = 0; index < count; ++index) {
            cdtp_status result;
            unsigned int bit = (unsigned int)((index * 7u + length) & 31u);
            words[index] ^= (uint32_t)1u << bit;
            result = cdtp_decode(words, count, &output);
            if (result == CDTP_OK) {
                fprintf(stderr, "Accepted corruption length=%lu word=%lu\n",
                        (unsigned long)length, (unsigned long)index);
                return 1;
            }
            words[index] ^= (uint32_t)1u << bit;
        }

        CHECK(cdtp_decode(words, count, &output) == CDTP_OK);
        if (count > CDTP_HEADER_WORDS + 1u) {
            words[CDTP_HEADER_WORDS] ^= 0x01010101u;
            CHECK(cdtp_decode(words, count, &output) != CDTP_OK);
            words[CDTP_HEADER_WORDS] ^= 0x01010101u;
        }
    }
    return 0;
}

static int test_zero_capacity_and_invalid_shapes(void)
{
    cdtp_packet packet, output;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count = 0;
    memset(&packet, 0, sizeof(packet));
    packet.payload_length = 0;
    CHECK(cdtp_encode(&packet, words, 3u, &count) == CDTP_ERR_CAPACITY);
    CHECK(count == 0u);
    CHECK(cdtp_encode(&packet, words, 4u, &count) == CDTP_OK);
    CHECK(cdtp_decode(words, 0u, &output) == CDTP_ERR_FORMAT);
    CHECK(cdtp_decode(words, 3u, &output) == CDTP_ERR_FORMAT);
    CHECK(cdtp_decode(words, 4u, &output) == CDTP_OK);
    words[2] = 0xFFFFFFFFu;
    CHECK(cdtp_decode(words, 4u, &output) == CDTP_ERR_TOO_LARGE);
    return 0;
}

int main(void)
{
    if (test_packet_matrix() || test_zero_capacity_and_invalid_shapes())
        return 1;
    puts("All payload lengths and per-word corruption checks passed.");
    return 0;
}
