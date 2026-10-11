#include "recovery_packet.h"
#include <string.h>

static uint8_t sample_byte(unsigned int sequence, unsigned int index)
{
    static const uint8_t sample[7] = {0x43u,0x44u,0x42u,0x4Bu,0x10u,0x20u,0x30u};
    return (uint8_t)(sample[index] ^ (uint8_t)((sequence - 1u) * 17u));
}
static int build(cdrp_tx *tx)
{
    cdgu_message message;
    memset(&message, 0, sizeof(message));
    message.session_id = CDRP_TRANSFER(tx->id);
    message.opcode = 1u;
    message.sequence = (uint16_t)tx->sequence;
    message.payload_length = 7u;
    for (unsigned int i = 0u; i < 7u; ++i)
        message.payload[i] = sample_byte(tx->sequence, i);
    tx->index = 0u;
    return cdgu_encode(&message, tx->words, CDRP_WORDS, &tx->count);
}
int cdrp_tx_init(cdrp_tx *tx, uint32_t id)
{
    if (!tx || !id || id > 0xFFFFu) return -1;
    memset(tx, 0, sizeof(*tx));
    tx->id = id;
    tx->sequence = 1u;
    return build(tx);
}
int cdrp_tx_next(cdrp_tx *tx, uint32_t now, uint32_t *word)
{
    if (!tx || !word || tx->phase == CDRP_TX_FAILED) return -1;
    if (tx->awaiting || tx->phase == CDRP_TX_DONE) return 0;
    if (tx->phase == CDRP_TX_GAP) {
        if ((uint32_t)(now - tx->wait_tick) < CDRP_GAP) return 0;
        tx->phase = CDRP_TX_QUERY;
    }
    switch (tx->phase) {
    case CDRP_TX_SESSION: *word = CDRP_SESSION | tx->id; break;
    case CDRP_TX_FRAME: *word = CDRP_FRAME | CDRP_KEY(tx->id, tx->sequence); break;
    case CDRP_TX_DATA:
        if (tx->index >= tx->count) return -1;
        *word = tx->high_half ? CDRP_HIGH | (tx->words[tx->index] >> 16) :
                CDRP_LOW | (tx->words[tx->index] & 0xFFFFu);
        break;
    case CDRP_TX_QUERY: *word = CDRP_QUERY | CDRP_KEY(tx->id, tx->sequence); break;
    default: return -1;
    }
    tx->last_word = *word;
    tx->awaiting = 1u;
    return 1;
}
static int retry(cdrp_tx *tx)
{
    if (++tx->retries > CDRP_RETRIES) {
        tx->phase = CDRP_TX_FAILED;
        return -1;
    }
    ++tx->recoveries;
    tx->index = tx->high_half = 0u;
    tx->phase = CDRP_TX_FRAME;
    return 1;
}
int cdrp_tx_reply(cdrp_tx *tx, uint32_t word, uint32_t now)
{
    if (!tx || tx->phase == CDRP_TX_FAILED) return -1;
    if (!tx->awaiting) return 0;
    tx->awaiting = 0u;
    if (tx->phase == CDRP_TX_QUERY) {
        if (word != (CDRP_ACK | CDRP_KEY(tx->id, tx->sequence)))
            return retry(tx); /* includes stale/wrong-session verdicts */
        ++tx->accepted;
        tx->retries = 0u;
        if (tx->sequence == CDRP_PACKETS) tx->phase = CDRP_TX_DONE;
        else {
            ++tx->sequence;
            tx->phase = CDRP_TX_FRAME;
            if (build(tx) != 0) { tx->phase = CDRP_TX_FAILED; return -1; }
        }
        return 1;
    }
    if (word != (tx->last_word ^ CDRP_XOR)) {
        /* Session negotiation failure cannot be repaired with FRAME alone. */
        if (tx->phase == CDRP_TX_SESSION) {
            tx->phase = CDRP_TX_FAILED;
            return -1;
        }
        return retry(tx);
    }
    if (tx->phase == CDRP_TX_SESSION) tx->phase = CDRP_TX_FRAME;
    else if (tx->phase == CDRP_TX_FRAME) tx->phase = CDRP_TX_DATA;
    else if (tx->phase == CDRP_TX_DATA) {
        if (!tx->high_half) tx->high_half = 1u;
        else {
            tx->high_half = 0u;
            if (++tx->index == tx->count) {
                tx->phase = CDRP_TX_GAP;
                tx->wait_tick = now;
            }
        }
    } else { tx->phase = CDRP_TX_FAILED; return -1; }
    return 1;
}
static int epoch(cdrp_peer *peer)
{
    if (peer->exhausted || peer->epoch == UINT32_MAX) {
        peer->exhausted = 1u;
        peer->armed = peer->pending = 0u;
        peer->reply = 0u;
        return -1;
    }
    ++peer->epoch;
    return 0;
}
void cdrp_peer_reset(cdrp_peer *peer)
{
    if (!peer || epoch(peer) != 0) return;
    peer->id = peer->frame_sequence = peer->count = 0u;
    peer->accepted = peer->forced_nack = peer->pending = peer->half_pending = 0u;
    peer->verdict = CDRP_PENDING;
    peer->reply = CDRP_READY;
    peer->armed = 1u;
}
uint32_t cdrp_peer_word(cdrp_peer *peer, uint32_t word)
{
    uint32_t kind, id, sequence;
    if (!peer || peer->exhausted) return 0u;
    kind = word & 0xFF000000u;
    id = (word >> 8) & 0xFFFFu;
    sequence = word & 0xFFu;
    if (kind == CDRP_SESSION) {
        if (peer->armed && !(word & 0x00FF0000u) && (word & 0xFFFFu)) {
            peer->id = word & 0xFFFFu;
            peer->armed = 0u;
        }
        /* An unarmed/incorrect handshake must not look acknowledged. */
        peer->reply = peer->id && peer->id == (word & 0x00FFFFFFu) ?
                      word ^ CDRP_XOR : 0u;
    } else if (kind == CDRP_FRAME) {
        if (peer->id && id == peer->id && sequence >= 1u &&
            sequence <= CDRP_PACKETS && epoch(peer) == 0) {
            peer->frame_sequence = sequence;
            peer->count = peer->pending = peer->half_pending = 0u;
            peer->verdict = CDRP_PENDING | CDRP_KEY(id, sequence);
        }
        peer->reply = peer->exhausted ? 0u : word ^ CDRP_XOR;
    } else if (kind == CDRP_QUERY) {
        peer->reply = peer->id && id == peer->id &&
                      sequence == peer->frame_sequence ?
                      peer->verdict : 0u;
    } else {
        peer->reply = word ^ CDRP_XOR;
        if (peer->id && peer->frame_sequence && !peer->pending &&
            peer->count < CDRP_WORDS && !(word & 0x00FF0000u)) {
            if (kind == CDRP_LOW) {
                peer->low_half = word & 0xFFFFu;
                peer->half_pending = 1u;
            } else if (kind == CDRP_HIGH && peer->half_pending) {
                uint32_t raw = ((word & 0xFFFFu) << 16) | peer->low_half;
                peer->half_pending = 0u;
                if (peer->count || raw == CDTP_MAGIC) {
                    peer->words[peer->count++] = raw;
                    if (peer->count == CDRP_WORDS) peer->pending = 1u;
                }
            }
        }
    }
    return peer->reply;
}
int cdrp_peer_take(cdrp_peer *peer, cdrp_work *work)
{
    if (!peer || !work || !peer->pending) return 0;
    work->epoch = peer->epoch;
    work->id = peer->id;
    work->frame_sequence = peer->frame_sequence;
    work->count = peer->count;
    memcpy(work->words, peer->words, sizeof(work->words));
    /* Keep pending set: IRQ must not reuse this frame until publish or
     * a new FRAME/RESET invalidates it. */
    return 1;
}
void cdrp_peer_validate(const cdrp_work *work, cdrp_result *result)
{
    cdgu_message message;
    if (!work || !result) return;
    result->epoch = work->epoch;
    result->id = work->id;
    result->sequence = work->frame_sequence;
    result->valid = 0;
    if (!work->id || work->id > 0xFFFFu || work->count != CDRP_WORDS ||
        work->frame_sequence < 1u || work->frame_sequence > CDRP_PACKETS)
        return;
    if (cdgu_decode(work->words, work->count, CDRP_TRANSFER(work->id), &message) != 0 ||
        message.opcode != 1u || message.sequence != work->frame_sequence ||
        message.payload_length != 7u) return;
    for (unsigned int i = 0; i < 7u; ++i)
        if (message.payload[i] != sample_byte(message.sequence, i)) return;
    result->valid = 1;
}
int cdrp_peer_publish(cdrp_peer *peer, const cdrp_result *result)
{
    uint32_t key;
    if (!peer || !result || peer->exhausted || !peer->pending ||
        result->epoch != peer->epoch || result->id != peer->id ||
        result->sequence != peer->frame_sequence) return 0;
    peer->pending = 0u;
    key = CDRP_KEY(peer->id, peer->frame_sequence);
    if (!result->valid || result->sequence > peer->accepted + 1u ||
        result->sequence < peer->accepted) {
        peer->verdict = CDRP_NACK | key;
    } else if (result->sequence == peer->accepted) {
        peer->verdict = CDRP_ACK | key; /* duplicate after lost verdict */
    } else if (result->sequence == 2u && !peer->forced_nack) {
        peer->forced_nack = 1u;
        peer->verdict = CDRP_NACK | key;
    } else {
        ++peer->accepted;
        peer->verdict = CDRP_ACK | key;
    }
    return 1;
}
