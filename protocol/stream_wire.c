#include "cubedeck/stream_wire.h"

static int reply_for(uint8_t opcode,
                     uint16_t sequence, cdtp_packet *reply)
{
    if (cdrl_make_reply(opcode, sequence, reply) != 0)
        return -1;
    return opcode == CDRL_OP_ACK ? 1 : 0;
}

int cdstw_receive_decoded(cdst_receiver *receiver,
                          const cdtp_packet *packet, cdtp_packet *reply)
{
    cdst_result result;
    if (!receiver || !packet || !reply)
        return -1;
    result = cdst_receive(receiver, packet);
    if (result == CDST_ACCEPTED || result == CDST_DUPLICATE ||
        result == CDST_VERIFIED)
        return reply_for(CDRL_OP_ACK, packet->sequence, reply);
    /* At maximum sequence 65535, only FINISH may be expected. */
    return reply_for(CDRL_OP_NACK,
                     (uint16_t)receiver->next_sequence, reply);
}

int cdstw_receive_frame(cdst_receiver *receiver,
                        const uint32_t *words, size_t word_count,
                        cdtp_packet *reply)
{
    cdtp_packet packet;
    if (!receiver || !words || !reply)
        return -1;
    if (cdtp_decode(words, word_count, &packet) != CDTP_OK)
        return reply_for(CDRL_OP_NACK,
                         (uint16_t)receiver->next_sequence, reply);
    return cdstw_receive_decoded(receiver, &packet, reply);
}
