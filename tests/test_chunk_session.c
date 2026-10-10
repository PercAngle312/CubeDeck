#include "cubedeck/chunk_session.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int test_round_trip(void)
{
    cdcs_receiver receiver;
    cdtp_packet tx, rx;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    size_t index;
    cdcs_receiver_init(&receiver);
    memset(&tx, 0, sizeof(tx));
    tx.opcode = CDCS_OP_DATA;
    for (index = 0; index < 18u; ++index) {
        size_t i;
        tx.sequence = (uint16_t)index;
        tx.payload_length = (uint16_t)((index == 17u) ? 1u : 256u);
        for (i = 0; i < tx.payload_length; ++i)
            tx.payload[i] = (uint8_t)(index + i);
        CHECK(cdtp_encode(&tx, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
        CHECK(cdtp_decode(words, count, &rx) == CDTP_OK);
        if (index < 16u) {
            CHECK(cdcs_receive(&receiver, &rx) == CDCS_ACCEPTED);
            CHECK(cdcs_receive(&receiver, &rx) == CDCS_DUPLICATE);
        } else {
            CHECK(cdcs_receive(&receiver, &rx) == CDCS_ERR_CAPACITY);
            CHECK(receiver.length == CDCS_MAX_IMAGE);
            CHECK(receiver.next_sequence == 16u);
            break;
        }
    }
    CHECK(receiver.length == CDCS_MAX_IMAGE);
    for (index = 0; index < 16u; ++index)
        CHECK(receiver.data[index * 256u] == (uint8_t)index);
    return 0;
}

static int test_rejected_frames(void)
{
    cdcs_receiver receiver;
    cdtp_packet packet;
    size_t original_length;
    cdcs_receiver_init(&receiver);
    memset(&packet, 0, sizeof(packet));
    packet.opcode = CDCS_OP_DATA;
    packet.payload_length = 3u;
    packet.payload[0] = 7u;
    CHECK(cdcs_receive(NULL, &packet) == CDCS_ERR_ARGUMENT);
    CHECK(cdcs_receive(&receiver, NULL) == CDCS_ERR_ARGUMENT);
    packet.sequence = 1u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ERR_SEQUENCE);
    packet.sequence = 0u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ACCEPTED);
    original_length = receiver.length;
    packet.payload[0] = 8u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ERR_CONTENT);
    CHECK(receiver.length == original_length);
    packet.sequence = 2u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ERR_SEQUENCE);
    packet.sequence = 1u;
    packet.opcode = 0u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ERR_OPCODE);
    packet.opcode = CDCS_OP_DATA;
    packet.payload_length = 0u;
    CHECK(cdcs_receive(&receiver, &packet) == CDCS_ERR_CONTENT);
    CHECK(receiver.next_sequence == 1u);
    return 0;
}

static int test_corrupt_wire_frame(void)
{
    cdtp_packet packet, decoded;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    memset(&packet, 0, sizeof(packet));
    packet.opcode = CDCS_OP_DATA;
    packet.payload_length = 4u;
    packet.payload[0] = 0x55u;
    CHECK(cdtp_encode(&packet, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    words[3] ^= 0x0100u;
    CHECK(cdtp_decode(words, count, &decoded) == CDTP_ERR_CRC);
    return 0;
}

int main(void)
{
    if (test_round_trip() || test_rejected_frames() || test_corrupt_wire_frame())
        return 1;
    puts("Chunk-session host tests passed.");
    return 0;
}
