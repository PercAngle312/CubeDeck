#include "cubedeck/verified_session.h"
#include <string.h>

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u32_le(uint8_t *p, uint32_t value)
{
    unsigned int i;
    for (i = 0u; i < 4u; ++i)
        p[i] = (uint8_t)(value >> (8u * i));
}

void cdvs_init(cdvs_receiver *receiver)
{
    if (receiver != NULL)
        memset(receiver, 0, sizeof(*receiver));
}

int cdvs_make_manifest(size_t length, uint32_t crc32, cdtp_packet *packet)
{
    if (!packet || length > CDCS_MAX_IMAGE)
        return -1;
    memset(packet, 0, sizeof(*packet));
    packet->opcode = CDVS_OP_MANIFEST;
    packet->payload_length = CDVS_MANIFEST_BYTES;
    write_u32_le(packet->payload, (uint32_t)length);
    write_u32_le(packet->payload + 4u, crc32);
    return 0;
}

int cdvs_make_finish(uint16_t sequence, cdtp_packet *packet)
{
    if (!packet)
        return -1;
    memset(packet, 0, sizeof(*packet));
    packet->opcode = CDVS_OP_FINISH;
    packet->sequence = sequence;
    return 0;
}

cdvs_result cdvs_receive(cdvs_receiver *receiver, const cdtp_packet *packet)
{
    cdcs_result chunk_status;
    if (!receiver || !packet)
        return CDVS_ERR_ARGUMENT;
    if (packet->opcode == CDVS_OP_MANIFEST) {
        uint32_t declared_length;
        uint32_t declared_crc;
        if (packet->sequence != 0u ||
            packet->payload_length != CDVS_MANIFEST_BYTES)
            return CDVS_ERR_MANIFEST;
        declared_length = read_u32_le(packet->payload);
        declared_crc = read_u32_le(packet->payload + 4u);
        if (declared_length > CDCS_MAX_IMAGE)
            return CDVS_ERR_CAPACITY;
        if (receiver->has_manifest) {
            return receiver->manifest.expected_bytes == declared_length &&
                   receiver->manifest.expected_crc32 == declared_crc ?
                   CDVS_DUPLICATE : CDVS_ERR_MANIFEST;
        }
        receiver->manifest.expected_bytes = declared_length;
        receiver->manifest.expected_crc32 = declared_crc;
        receiver->has_manifest = 1;
        return CDVS_ACCEPTED;
    }
    if (!receiver->has_manifest)
        return CDVS_ERR_STATE;
    if (packet->opcode == CDVS_OP_FINISH) {
        int result;
        if (packet->payload_length != 0u ||
            packet->sequence != receiver->chunks.next_sequence)
            return CDVS_ERR_SEQUENCE;
        result = cdim_verify(&receiver->manifest,
                             receiver->chunks.data,
                             receiver->chunks.length);
        if (result == 1) {
            receiver->verified = 1;
            return CDVS_VERIFIED;
        }
        return CDVS_ERR_INTEGRITY;
    }
    if (packet->opcode != CDCS_OP_DATA || receiver->verified)
        return CDVS_ERR_STATE;
    if (packet->payload_length == 0u ||
        packet->payload_length > receiver->manifest.expected_bytes ||
        receiver->chunks.length >
            receiver->manifest.expected_bytes - packet->payload_length)
        return CDVS_ERR_CAPACITY;
    chunk_status = cdcs_receive(&receiver->chunks, packet);
    if (chunk_status == CDCS_ACCEPTED)
        return CDVS_ACCEPTED;
    if (chunk_status == CDCS_DUPLICATE)
        return CDVS_DUPLICATE;
    if (chunk_status == CDCS_ERR_SEQUENCE)
        return CDVS_ERR_SEQUENCE;
    return CDVS_ERR_CAPACITY;
}
