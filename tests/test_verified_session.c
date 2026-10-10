#include "cubedeck/verified_session.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int run_manifest_session(void)
{
    uint8_t source[CDCS_MAX_IMAGE], altered[CDCS_MAX_IMAGE];
    cdvs_receiver receiver;
    cdtp_packet tx, rx;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count, index;
    uint32_t checksum;
    for (index = 0; index < sizeof(source); ++index)
        source[index] = (uint8_t)((index * 7u) ^ (index >> 5));
    checksum = cdim_crc32(source, sizeof(source));
    memcpy(altered, source, sizeof(source));
    cdvs_init(&receiver);
    memset(&tx, 0, sizeof(tx));
    tx.opcode = CDCS_OP_DATA;
    tx.payload_length = 1u;
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ERR_STATE);
    CHECK(cdvs_make_manifest(sizeof(source), checksum, &tx) == 0);
    CHECK(cdtp_encode(&tx, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
    CHECK(cdtp_decode(words, count, &rx) == CDTP_OK);
    CHECK(cdvs_receive(&receiver, &rx) == CDVS_ACCEPTED);
    CHECK(cdvs_receive(&receiver, &rx) == CDVS_DUPLICATE);
    for (index = 0; index < 16u; ++index) {
        memset(&tx, 0, sizeof(tx));
        tx.opcode = CDCS_OP_DATA;
        tx.sequence = (uint16_t)index;
        tx.payload_length = 256u;
        memcpy(tx.payload, source + index * 256u, 256u);
        CHECK(cdtp_encode(&tx, words, CDTP_MAX_PACKET_WORDS, &count) == CDTP_OK);
        CHECK(cdtp_decode(words, count, &rx) == CDTP_OK);
        CHECK(cdvs_receive(&receiver, &rx) == CDVS_ACCEPTED);
        CHECK(cdvs_receive(&receiver, &rx) == CDVS_DUPLICATE);
    }
    CHECK(receiver.chunks.length == sizeof(source));
    CHECK(receiver.verified == 0);
    CHECK(cdvs_make_finish(15u, &tx) == 0);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ERR_SEQUENCE);
    CHECK(cdvs_make_finish(16u, &tx) == 0);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_VERIFIED);
    CHECK(receiver.verified == 1);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_VERIFIED);
    memset(&tx, 0, sizeof(tx));
    tx.opcode = CDCS_OP_DATA;
    tx.sequence = 16u;
    tx.payload_length = 1u;
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ERR_STATE);
    CHECK(cdvs_make_manifest(sizeof(source), checksum ^ 1u, &tx) == 0);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ERR_MANIFEST);
    cdvs_init(&receiver);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ACCEPTED);
    for (index = 0; index < 16u; ++index) {
        memset(&tx, 0, sizeof(tx));
        tx.opcode = CDCS_OP_DATA;
        tx.sequence = (uint16_t)index;
        tx.payload_length = 256u;
        memcpy(tx.payload, altered + index * 256u, 256u);
        CHECK(cdvs_receive(&receiver, &tx) == CDVS_ACCEPTED);
    }
    CHECK(cdvs_make_finish(16u, &tx) == 0);
    CHECK(cdvs_receive(&receiver, &tx) == CDVS_ERR_INTEGRITY);
    CHECK(receiver.verified == 0);
    return 0;
}

static int test_rejections(void)
{
    cdvs_receiver receiver;
    cdtp_packet packet;
    cdvs_init(&receiver);
    CHECK(cdvs_receive(NULL, &packet) == CDVS_ERR_ARGUMENT);
    CHECK(cdvs_receive(&receiver, NULL) == CDVS_ERR_ARGUMENT);
    CHECK(cdvs_make_manifest(CDCS_MAX_IMAGE + 1u, 0u, &packet) == -1);
    CHECK(cdvs_make_manifest(7u, 0u, &packet) == 0);
    packet.sequence = 1u;
    CHECK(cdvs_receive(&receiver, &packet) == CDVS_ERR_MANIFEST);
    packet.sequence = 0u;
    packet.payload[3] = 0x01u;
    CHECK(cdvs_receive(&receiver, &packet) == CDVS_ERR_CAPACITY);
    packet.payload[3] = 0u;
    CHECK(cdvs_receive(&receiver, &packet) == CDVS_ACCEPTED);
    CHECK(cdvs_make_finish(0u, &packet) == 0);
    CHECK(cdvs_receive(&receiver, &packet) == CDVS_ERR_INTEGRITY);
    memset(&packet, 0, sizeof(packet));
    packet.opcode = CDCS_OP_DATA;
    packet.payload_length = 8u;
    CHECK(cdvs_receive(&receiver, &packet) == CDVS_ERR_CAPACITY);
    return 0;
}

int main(void)
{
    if (run_manifest_session() || test_rejections())
        return 1;
    puts("Manifest-first verified transfer tests passed.");
    return 0;
}
