#include "cubedeck/session_sender.h"
#include "cubedeck/session_wire.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int deliver(cdvs_receiver *receiver, const cdtp_packet *packet,
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

static int transfer(size_t size)
{
    uint8_t source[CDCS_MAX_IMAGE];
    cdss_sender sender;
    cdvs_receiver receiver;
    cdtp_packet packet, reply;
    size_t i, iterations = 0;
    int damage_done = 0, loss_done = 0;
    for (i = 0; i < size; ++i)
        source[i] = (uint8_t)(i * 29u + (i >> 4));
    cdvs_init(&receiver);
    CHECK(cdss_init(&sender, source, size) == 0);
    while (sender.phase != CDSS_COMPLETE) {
        CHECK(++iterations < 100u);
        CHECK(cdss_next(&sender, &packet) == 1);
        if (!damage_done && packet.opcode == CDCS_OP_DATA) {
            CHECK(deliver(&receiver, &packet, &reply, 1) == 0);
            CHECK(reply.opcode == CDRL_OP_NACK);
            CHECK(cdss_reply(&sender, &reply) == 0);
            CHECK(sender.retries == 1u);
            damage_done = 1;
        }
        CHECK(deliver(&receiver, &packet, &reply, 0) == 1);
        CHECK(reply.opcode == CDRL_OP_ACK);
        if (!loss_done && packet.opcode == CDCS_OP_DATA) {
            size_t unchanged = receiver.chunks.length;
            CHECK(cdss_timeout(&sender) == 0);
            CHECK(deliver(&receiver, &packet, &reply, 0) == 1);
            CHECK(receiver.chunks.length == unchanged);
            loss_done = 1;
        }
        CHECK(cdss_reply(&sender, &reply) == 1);
    }
    CHECK(cdss_next(&sender, &packet) == 0);
    CHECK(receiver.verified);
    CHECK(receiver.chunks.length == size);
    CHECK(memcmp(receiver.chunks.data, source, size) == 0);
    return 0;
}

static int test_retry_limits(void)
{
    cdss_sender s;
    cdtp_packet p, reply;
    uint8_t data[] = {0x33u};
    unsigned int i;
    CHECK(cdss_init(NULL, data, 1u) == -1);
    CHECK(cdss_init(&s, NULL, 1u) == -1);
    CHECK(cdss_init(&s, data, CDCS_MAX_IMAGE + 1u) == -1);
    CHECK(cdss_init(&s, data, sizeof(data)) == 0);
    CHECK(cdss_next(&s, &p) == 1);
    CHECK(cdrl_make_reply(CDRL_OP_ACK, 2u, &reply) == 0);
    CHECK(cdss_reply(&s, &reply) == 0);
    CHECK(s.phase == CDSS_MANIFEST);
    for (i = 0; i < CDRL_MAX_RETRIES; ++i)
        CHECK(cdss_timeout(&s) == 0);
    CHECK(cdss_timeout(&s) == -1);
    CHECK(s.phase == CDSS_FAILED);
    CHECK(cdss_next(&s, &p) == -1);
    return 0;
}

int main(void)
{
    if (transfer(0u) || transfer(1u) || transfer(257u) ||
        transfer(CDCS_MAX_IMAGE) || test_retry_limits())
        return 1;
    puts("Full manifest to FINISH sender/receiver simulation passed.");
    return 0;
}
