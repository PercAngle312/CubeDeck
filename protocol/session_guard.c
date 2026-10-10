#include "cubedeck/session_guard.h"
#include "cubedeck/reliable_link.h"
#include <string.h>

static void write_id(uint8_t *p, uint32_t id)
{
    unsigned int i;
    for (i = 0u; i < 4u; ++i)
        p[i] = (uint8_t)(id >> (8u * i));
}

static uint32_t read_id(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int cdgu_encode(const cdgu_message *message, uint32_t *words,
                size_t capacity, size_t *out_count)
{
    cdtp_packet packet;
    if (!message || !words || !out_count || !message->session_id ||
        message->payload_length > CDGU_MAX_PAYLOAD)
        return -1;
    memset(&packet, 0, sizeof(packet));
    packet.opcode = message->opcode;
    packet.sequence = message->sequence;
    packet.payload_length = message->payload_length + CDGU_SESSION_BYTES;
    write_id(packet.payload, message->session_id);
    memcpy(packet.payload + CDGU_SESSION_BYTES, message->payload,
           message->payload_length);
    return cdtp_encode(&packet, words, capacity, out_count) == CDTP_OK ?
           0 : -1;
}

int cdgu_decode(const uint32_t *words, size_t count,
                uint32_t expected_session, cdgu_message *message)
{
    cdtp_packet packet;
    uint32_t id;
    if (!words || !message || !expected_session)
        return -1;
    if (cdtp_decode(words, count, &packet) != CDTP_OK ||
        packet.payload_length < CDGU_SESSION_BYTES)
        return -2;
    id = read_id(packet.payload);
    if (!id || id != expected_session)
        return -2;
    memset(message, 0, sizeof(*message));
    message->session_id = id;
    message->opcode = packet.opcode;
    message->sequence = packet.sequence;
    message->payload_length = packet.payload_length - CDGU_SESSION_BYTES;
    memcpy(message->payload, packet.payload + CDGU_SESSION_BYTES,
           message->payload_length);
    return 0;
}

int cdgu_ack_matches(const cdgu_message *request,
                     const cdgu_message *ack)
{
    if (!request || !ack || !request->session_id)
        return -1;
    return ack->session_id == request->session_id &&
           ack->opcode == CDRL_OP_ACK &&
           ack->sequence == request->sequence &&
           ack->payload_length == 1u &&
           ack->payload[0] == request->opcode ? 1 : 0;
}
