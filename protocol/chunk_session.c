#include "cubedeck/chunk_session.h"
#include <string.h>

void cdcs_receiver_init(cdcs_receiver *receiver)
{
    if (receiver != NULL)
        memset(receiver, 0, sizeof(*receiver));
}

cdcs_result cdcs_receive(cdcs_receiver *receiver, const cdtp_packet *packet)
{
    if (receiver == NULL || packet == NULL)
        return CDCS_ERR_ARGUMENT;
    if (packet->opcode != CDCS_OP_DATA)
        return CDCS_ERR_OPCODE;
    if (packet->payload_length == 0u || packet->payload_length > CDTP_MAX_PAYLOAD)
        return CDCS_ERR_CONTENT;

    if (receiver->has_last && packet->sequence == receiver->last_sequence) {
        if (packet->payload_length == receiver->last_length &&
            memcmp(packet->payload, receiver->last_payload,
                   packet->payload_length) == 0)
            return CDCS_DUPLICATE;
        return CDCS_ERR_CONTENT;
    }
    if (packet->sequence != receiver->next_sequence)
        return CDCS_ERR_SEQUENCE;
    if (receiver->length > CDCS_MAX_IMAGE ||
        packet->payload_length > CDCS_MAX_IMAGE - receiver->length)
        return CDCS_ERR_CAPACITY;

    memcpy(receiver->data + receiver->length,
           packet->payload, packet->payload_length);
    receiver->length += packet->payload_length;
    receiver->last_sequence = packet->sequence;
    receiver->last_length = packet->payload_length;
    memcpy(receiver->last_payload, packet->payload, packet->payload_length);
    receiver->has_last = 1;
    receiver->next_sequence = (uint16_t)(receiver->next_sequence + 1u);
    return CDCS_ACCEPTED;
}
