#include "cubedeck/word_exchange.h"
#include <string.h>

static int start_packet(cdwx_link *link)
{
    int status;
    if (link->sender->phase == CDTX_COMPLETE) {
        link->phase = CDWX_COMPLETE;
        return 0;
    }
    status = cdbs_next_frame(link->sender, link->session_id, link->words,
                             CDTP_MAX_PACKET_WORDS, &link->word_count);
    if (status != 1) {
        link->phase = status == 0 ? CDWX_COMPLETE : CDWX_FAILED;
        return status == 0 ? 0 : -1;
    }
    link->next_word = 0u;
    link->phase = CDWX_SENDING;
    return 1;
}

int cdwx_init(cdwx_link *link, cdtx_sender *sender, uint32_t session_id)
{
    if (!link || !sender || !session_id)
        return -1;
    memset(link, 0, sizeof(*link));
    link->sender = sender;
    link->session_id = session_id;
    cdps_init(&link->replies);
    return start_packet(link) < 0 ? -1 : 0;
}

int cdwx_next_word(cdwx_link *link, uint32_t *word)
{
    if (!link || !word || link->phase == CDWX_FAILED)
        return -1;
    if (link->phase != CDWX_SENDING)
        return 0;
    if (link->next_word >= link->word_count) {
        link->phase = CDWX_FAILED;
        return -1;
    }
    *word = link->words[link->next_word++];
    if (link->next_word == link->word_count)
        link->phase = CDWX_WAIT_REPLY;
    return 1;
}

int cdwx_reply_word(cdwx_link *link, uint32_t word)
{
    cdtp_packet packet;
    uint32_t words[CDTP_MAX_PACKET_WORDS];
    size_t count;
    int result;
    if (!link || link->phase == CDWX_FAILED)
        return -1;
    if (link->phase != CDWX_WAIT_REPLY)
        return 0;
    result = cdps_feed(&link->replies, word, &packet);
    if (result <= 0)
        return 0;
    /* The word parser validated CRC; reconstruct the same verified frame
     * for the session-bound ACK verifier. This is host-portable, avoids
     * trusting a caller-supplied ACK struct, and allocates only one frame.
     */
    if (cdtp_encode(&packet, words, CDTP_MAX_PACKET_WORDS,
                    &count) != CDTP_OK) {
        link->phase = CDWX_FAILED;
        return -1;
    }
    result = cdbs_sender_reply(link->sender, link->session_id, words, count);
    if (result < 0) {
        link->phase = CDWX_FAILED;
        return -1;
    }
    if (result == 0) {
        cdgu_message decoded;
        /* A current-session NACK has already consumed one retry in
         * cdtx_reply. Replay immediately, without charging the retry
         * budget again. A delayed/unrelated NACK must not restart TX. */
        if (cdgu_decode(words, count, link->session_id, &decoded) == 0 &&
            decoded.opcode == CDRL_OP_NACK &&
            decoded.payload_length == 0u &&
            link->sender->pending_valid &&
            decoded.sequence == link->sender->pending.sequence) {
            cdps_init(&link->replies);
            link->next_word = 0u;
            link->phase = CDWX_SENDING;
        }
        return 0;
    }
    cdps_init(&link->replies);
    return start_packet(link) < 0 ? -1 : 1;
}

int cdwx_timeout(cdwx_link *link)
{
    if (!link || (link->phase != CDWX_WAIT_REPLY &&
                  link->phase != CDWX_SENDING))
        return -1;
    if (cdtx_timeout(link->sender) != 0) {
        link->phase = CDWX_FAILED;
        return -1;
    }
    cdps_init(&link->replies);
    link->next_word = 0u;
    link->phase = CDWX_SENDING;
    return 0;
}
