#include "cubedeck/reliable_link.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int send_wire(cdcs_receiver *receiver, cdtp_packet *data,
                     cdtp_packet *reply, int corrupt)
{
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    CHECK(cdtp_encode(data, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    if (corrupt)
        words[3] ^= 0x01u;
    return cdrl_receive_frame(receiver, words, count, reply);
}

static int test_transfer_with_loss_and_damage(void)
{
    uint8_t source[CDCS_MAX_IMAGE];
    cdcs_receiver receiver;
    cdrl_sender sender;
    cdtp_packet outbound, reply, stray;
    size_t i, packet_index = 0;
    for (i = 0; i < sizeof(source); ++i)
        source[i] = (uint8_t)(i * 17u + (i >> 8));
    cdcs_receiver_init(&receiver);
    CHECK(cdrl_sender_init(&sender, source, sizeof(source)) == 0);
    while (sender.state != CDRL_DONE) {
        CHECK(packet_index < 20u);
        CHECK(cdrl_sender_next(&sender, &outbound) == 1);
        CHECK(outbound.sequence == packet_index);
        if (packet_index == 2u) {
            CHECK(send_wire(&receiver, &outbound, &reply, 1) == 0);
            CHECK(reply.opcode == CDRL_OP_NACK);
            CHECK(reply.sequence == outbound.sequence);
            CHECK(cdrl_sender_reply(&sender, &reply) == 0);
            CHECK(sender.retries == 1u);
        }
        CHECK(send_wire(&receiver, &outbound, &reply, 0) == 1);
        CHECK(reply.opcode == CDRL_OP_ACK);
        if (packet_index == 5u) {
            /* Lost ACK: retry must be accepted without duplicate append. */
            size_t old_length = receiver.length;
            CHECK(cdrl_sender_timeout(&sender) == 0);
            CHECK(cdrl_sender_next(&sender, &outbound) == 1);
            CHECK(send_wire(&receiver, &outbound, &reply, 0) == 1);
            CHECK(receiver.length == old_length);
        }
        CHECK(cdrl_make_reply(CDRL_OP_ACK, (uint16_t)(outbound.sequence + 1u), &stray) == 0);
        CHECK(cdrl_sender_reply(&sender, &stray) == 0);
        CHECK(sender.offset == packet_index * CDTP_MAX_PAYLOAD);
        CHECK(cdrl_sender_reply(&sender, &reply) == 1);
        ++packet_index;
    }
    CHECK(packet_index == 16u);
    CHECK(receiver.length == sizeof(source));
    CHECK(memcmp(receiver.data, source, sizeof(source)) == 0);
    CHECK(sender.offset == sizeof(source));
    return 0;
}

static int test_retry_limit_and_validation(void)
{
    uint8_t bytes[2] = {1u, 2u};
    cdrl_sender sender;
    cdtp_packet packet, reply;
    unsigned int i;
    CHECK(cdrl_sender_init(NULL, bytes, sizeof(bytes)) == -1);
    CHECK(cdrl_sender_init(&sender, NULL, 1u) == -1);
    CHECK(cdrl_sender_init(&sender, bytes, CDCS_MAX_IMAGE + 1u) == -1);
    CHECK(cdrl_sender_init(&sender, bytes, 0u) == 0);
    CHECK(sender.state == CDRL_DONE);
    CHECK(cdrl_sender_next(&sender, &packet) == 0);
    CHECK(cdrl_sender_init(&sender, bytes, sizeof(bytes)) == 0);
    CHECK(cdrl_sender_next(&sender, &packet) == 1);
    for (i = 0; i < CDRL_MAX_RETRIES; ++i)
        CHECK(cdrl_sender_timeout(&sender) == 0);
    CHECK(cdrl_sender_timeout(&sender) == -1);
    CHECK(sender.state == CDRL_FAILED);
    CHECK(cdrl_sender_next(&sender, &packet) == -1);
    CHECK(cdrl_make_reply(0u, 0u, &reply) == -1);
    return 0;
}

int main(void)
{
    if (test_transfer_with_loss_and_damage() || test_retry_limit_and_validation())
        return 1;
    puts("Reliable-link host tests passed.");
    return 0;
}
