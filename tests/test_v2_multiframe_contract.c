#include "cubedeck/session_guard.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)
#define COUNT 3u
#define ID 0x42CD2026u

static const uint8_t sample[7] = {0x43u, 0x44u, 0x42u, 0x4Bu, 0x10u, 0x20u, 0x30u};

static int encode_frame(unsigned index, uint32_t *out, size_t *count)
{
    cdgu_message msg;
    unsigned i;
    memset(&msg, 0, sizeof(msg));
    msg.session_id = ID;
    msg.opcode = 1u;
    msg.sequence = (uint16_t)(index + 1u);
    msg.payload_length = 7u;
    for (i = 0u; i < 7u; ++i)
        msg.payload[i] = (uint8_t)(sample[i] ^ (uint8_t)(index * 17u));
    return cdgu_encode(&msg, out, CDTP_MAX_PACKET_WORDS, count);
}

static int accept_frame(const uint32_t *words, size_t count,
                        unsigned int *accepted)
{
    cdgu_message msg;
    unsigned i;
    if (cdgu_decode(words, count, ID, &msg) != 0 ||
        msg.opcode != 1u || msg.payload_length != 7u ||
        *accepted >= COUNT || msg.sequence != *accepted + 1u)
        return -1;
    for (i = 0; i < 7u; ++i)
        if (msg.payload[i] !=
            (uint8_t)(sample[i] ^ (uint8_t)(*accepted * 17u)))
            return -1;
    ++*accepted;
    return 0;
}

int main(void)
{
    uint32_t frames[COUNT][CDTP_MAX_PACKET_WORDS];
    size_t counts[COUNT];
    unsigned int accepted = 0u;
    unsigned i, j;
    for (i = 0u; i < COUNT; ++i) {
        CHECK(encode_frame(i, frames[i], &counts[i]) == 0);
        CHECK(counts[i] == 7u && frames[i][0] == CDTP_MAGIC);
    }
    CHECK(accept_frame(frames[1], counts[1], &accepted) != 0);
    CHECK(accepted == 0u);
    CHECK(accept_frame(frames[0], counts[0], &accepted) == 0);
    CHECK(accepted == 1u);
    CHECK(accept_frame(frames[0], counts[0], &accepted) != 0);
    CHECK(accepted == 1u);
    for (i = 1u; i < COUNT; ++i) {
        uint32_t tampered[CDTP_MAX_PACKET_WORDS];
        memcpy(tampered, frames[i], counts[i] * sizeof(uint32_t));
        tampered[counts[i] - 1u] ^= 1u;
        CHECK(accept_frame(tampered, counts[i], &accepted) != 0);
        CHECK(accepted == i);
        /* Identity-byte and payload corruption cannot silently pass CRC. */
        for (j = 3u; j < counts[i] - 1u; ++j) {
            memcpy(tampered, frames[i], counts[i] * sizeof(uint32_t));
            tampered[j] ^= 0x01000000u;
            CHECK(accept_frame(tampered, counts[i], &accepted) != 0);
            CHECK(accepted == i);
        }
        CHECK(accept_frame(frames[i], counts[i], &accepted) == 0);
    }
    CHECK(accepted == COUNT);
    CHECK(accept_frame(frames[2], counts[2], &accepted) != 0);
    puts("Three separate CRC-verified sequential v2 packet fixtures passed.");
    return 0;
}
