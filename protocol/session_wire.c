#include "cubedeck/session_wire.h"

int cdsw_receive_frame(cdvs_receiver *receiver, const uint32_t *words,
                       size_t word_count, cdtp_packet *reply)
{
    cdtp_packet packet;
    cdvs_result result;
    if (!receiver || !words || !reply)
        return -1;
    if (cdtp_decode(words, word_count, &packet) != CDTP_OK) {
        cdrl_make_reply(CDRL_OP_NACK, receiver->chunks.next_sequence, reply);
        return 0;
    }
    result = cdvs_receive(receiver, &packet);
    if (result == CDVS_ACCEPTED || result == CDVS_DUPLICATE ||
        result == CDVS_VERIFIED) {
        cdrl_make_reply(CDRL_OP_ACK, packet.sequence, reply);
        return 1;
    }
    cdrl_make_reply(CDRL_OP_NACK, receiver->chunks.next_sequence, reply);
    return 0;
}
