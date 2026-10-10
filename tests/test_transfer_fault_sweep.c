#include "cubedeck/session_sender.h"
#include "cubedeck/session_wire.h"
#include <stdio.h>
#include <string.h>

/* Deterministic fault sweep of complete host-only transfers.
 * Runs boundary sizes and diverse lengths, with dropped ACKs, CRC damage,
 * duplicate frames, and stale ACKs. No SI hardware or storage I/O.
 */
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fault sweep failed at line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int wire(cdvs_receiver *receiver, const cdtp_packet *packet,
                cdtp_packet *reply, int corrupt)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    if (cdtp_encode(packet, words, CDTP_MAX_PACKET_WORDS, &count) != CDTP_OK)
        return -1;
    if (corrupt)
        words[count - 1u] ^= 1u;
    return cdsw_receive_frame(receiver, words, count, reply);
}

static int test_case(size_t length, unsigned int seed)
{
    uint8_t source[CDCS_MAX_IMAGE];
    cdvs_receiver receiver;
    cdss_sender sender;
    cdtp_packet packet, reply, wrong;
    unsigned int frame = 0u;
    size_t i;
    for (i = 0u; i < length; ++i)
        source[i] = (uint8_t)((i * 41u) ^ (seed * 19u) ^ (i >> 4u));
    cdvs_init(&receiver);
    CHECK(cdss_init(&sender, source, length) == 0);
    while (sender.phase != CDSS_COMPLETE) {
        unsigned int selector;
        CHECK(frame++ < 64u);
        CHECK(cdss_next(&sender, &packet) == 1);
        selector = (frame + seed) % 7u;
        if (selector == 0u) {
            size_t before = receiver.chunks.length;
            CHECK(wire(&receiver, &packet, &reply, 1) == 0);
            CHECK(receiver.chunks.length == before);
            CHECK(reply.opcode == CDRL_OP_NACK);
            CHECK(cdss_reply(&sender, &reply) == 0);
        }
        CHECK(wire(&receiver, &packet, &reply, 0) == 1);
        CHECK(reply.opcode == CDRL_OP_ACK);
        CHECK(reply.sequence == packet.sequence);
        if (selector == 1u) {
            size_t before = receiver.chunks.length;
            /* Lost ACK: resend exact frame and check duplicate idempotence. */
            CHECK(cdss_timeout(&sender) == 0);
            CHECK(wire(&receiver, &packet, &reply, 0) == 1);
            CHECK(receiver.chunks.length == before);
        }
        if (selector == 2u) {
            CHECK(cdrl_make_reply(CDRL_OP_ACK,
                                 (uint16_t)(packet.sequence + 1u), &wrong) == 0);
            CHECK(cdss_reply(&sender, &wrong) == 0);
        }
        CHECK(cdss_reply(&sender, &reply) == 1);
    }
    CHECK(receiver.verified == 1);
    CHECK(receiver.chunks.length == length);
    CHECK(memcmp(receiver.chunks.data, source, length) == 0);
    return 0;
}

int main(void)
{
    static const size_t edges[] = {
        0u, 1u, 2u, 3u, 4u, 7u, 8u, 15u, 31u,
        255u, 256u, 257u, 511u, 512u, 513u, 1023u,
        1024u, 1025u, 2047u, 2048u, 2049u, 4095u, 4096u
    };
    size_t i;
    unsigned int seed;
    for (i = 0u; i < sizeof(edges) / sizeof(edges[0]); ++i)
        for (seed = 0u; seed < 5u; ++seed)
            if (test_case(edges[i], seed))
                return 1;
    for (i = 0u; i < 100u; ++i)
        if (test_case((i * 977u + 13u) % (CDCS_MAX_IMAGE + 1u),
                      (unsigned int)i))
            return 1;
    puts("215 deterministic end-to-end transfer fault cases passed.");
    return 0;
}
