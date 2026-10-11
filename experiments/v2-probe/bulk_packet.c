#include "bulk_packet.h"
#include <string.h>
_Static_assert(CDBR_IMAGE_BYTES % CDBR_CHUNK_BYTES == 0u, "whole chunks required");
_Static_assert(CDBR_PACKETS <= 255u, "wire sequence is eight bits");
_Static_assert(CDBR_CHUNK_BYTES <= CDGU_MAX_PAYLOAD, "chunk fits session frame");

uint8_t cdbr_expected_byte(size_t offset)
{
    uint32_t n = (uint32_t)offset;
    return (uint8_t)((n * 73u + (n >> 3) * 19u + 0x5Au) ^ (n >> 1));
}
static uint32_t image_crc_update(uint32_t crc, const uint8_t *bytes, size_t count)
{
    for (size_t i = 0u; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned int bit = 0u; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc;
}
static uint8_t sample_byte(unsigned int sequence, unsigned int index)
{
    return cdbr_expected_byte((sequence - 1u) * CDBR_CHUNK_BYTES + index);
}
static int build(cdbr_tx *tx)
{
    cdgu_message message;
    memset(&message, 0, sizeof(message));
    message.session_id = CDBR_TRANSFER(tx->id);
    message.opcode = 1u;
    message.sequence = (uint16_t)tx->sequence;
    message.payload_length = CDBR_CHUNK_BYTES;
    for (unsigned int i = 0u; i < CDBR_CHUNK_BYTES; ++i)
        message.payload[i] = sample_byte(tx->sequence, i);
    tx->index = 0u;
    return cdgu_encode(&message, tx->words, CDBR_WORDS, &tx->count);
}
int cdbr_tx_init(cdbr_tx *tx, uint32_t id)
{
    if (!tx || !id || id > 0xFFFFu) return -1;
    memset(tx, 0, sizeof(*tx));
    tx->id = id;
    tx->sequence = 1u;
    return build(tx);
}
int cdbr_tx_next(cdbr_tx *tx, uint32_t now, uint32_t *word)
{
    if (!tx || !word || tx->phase == CDBR_TX_FAILED) return -1;
    if (tx->awaiting || tx->phase == CDBR_TX_DONE) return 0;
    if (tx->phase == CDBR_TX_GAP) {
        if ((uint32_t)(now - tx->wait_tick) < CDBR_GAP) return 0;
        tx->phase = CDBR_TX_QUERY;
    }
    switch (tx->phase) {
    case CDBR_TX_SESSION: *word = CDBR_SESSION | tx->id; break;
    case CDBR_TX_FRAME: *word = CDBR_FRAME | CDBR_KEY(tx->id, tx->sequence); break;
    case CDBR_TX_DATA:
        if (tx->index >= tx->count) return -1;
        *word = tx->high_half ? CDBR_HIGH | (tx->words[tx->index] >> 16) :
                CDBR_LOW | (tx->words[tx->index] & 0xFFFFu);
        break;
    case CDBR_TX_QUERY: *word = CDBR_QUERY | CDBR_KEY(tx->id, tx->sequence); break;
    default: return -1;
    }
    tx->last_word = *word;
    tx->awaiting = 1u;
    return 1;
}
static int retry(cdbr_tx *tx)
{
    if (++tx->retries > CDBR_RETRIES) {
        tx->phase = CDBR_TX_FAILED;
        return -1;
    }
    ++tx->recoveries;
    tx->index = tx->high_half = 0u;
    tx->phase = CDBR_TX_FRAME;
    return 1;
}
int cdbr_tx_reply(cdbr_tx *tx, uint32_t word, uint32_t now)
{
    if (!tx || tx->phase == CDBR_TX_FAILED) return -1;
    if (!tx->awaiting) return 0;
    tx->awaiting = 0u;
    if (tx->phase == CDBR_TX_QUERY) {
        if (word != (CDBR_ACK | CDBR_KEY(tx->id, tx->sequence)))
            return retry(tx); /* includes stale/wrong-session verdicts */
        ++tx->accepted;
        tx->retries = 0u;
        if (tx->sequence == CDBR_PACKETS) tx->phase = CDBR_TX_DONE;
        else {
            ++tx->sequence;
            tx->phase = CDBR_TX_FRAME;
            if (build(tx) != 0) { tx->phase = CDBR_TX_FAILED; return -1; }
        }
        return 1;
    }
    if (word != (tx->last_word ^ CDBR_XOR)) {
        /* Session negotiation failure cannot be repaired with FRAME alone. */
        if (tx->phase == CDBR_TX_SESSION) {
            tx->phase = CDBR_TX_FAILED;
            return -1;
        }
        return retry(tx);
    }
    if (tx->phase == CDBR_TX_SESSION) tx->phase = CDBR_TX_FRAME;
    else if (tx->phase == CDBR_TX_FRAME) tx->phase = CDBR_TX_DATA;
    else if (tx->phase == CDBR_TX_DATA) {
        if (!tx->high_half) tx->high_half = 1u;
        else {
            tx->high_half = 0u;
            if (++tx->index == tx->count) {
                tx->phase = CDBR_TX_GAP;
                tx->wait_tick = now;
            }
        }
    } else { tx->phase = CDBR_TX_FAILED; return -1; }
    return 1;
}
static int epoch(cdbr_peer *peer)
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
void cdbr_peer_reset(cdbr_peer *peer)
{
    if (!peer || epoch(peer) != 0) return;
    peer->id = peer->frame_sequence = peer->count = 0u;
    peer->accepted = peer->forced_nack = peer->pending = peer->half_pending = 0u;
    peer->verdict = CDBR_PENDING;
    peer->reply = CDBR_READY;
    peer->armed = 1u;
    peer->image_crc = 0u; /* Old RAM bytes are invalid until newly accepted. */
}
uint32_t cdbr_peer_word(cdbr_peer *peer, uint32_t word)
{
    uint32_t kind, id, sequence;
    if (!peer || peer->exhausted) return 0u;
    kind = word & 0xFF000000u;
    id = (word >> 8) & 0xFFFFu;
    sequence = word & 0xFFu;
    if (kind == CDBR_SESSION) {
        if (peer->armed && !(word & 0x00FF0000u) && (word & 0xFFFFu)) {
            peer->id = word & 0xFFFFu;
            peer->armed = 0u;
        }
        /* An unarmed/incorrect handshake must not look acknowledged. */
        peer->reply = peer->id && peer->id == (word & 0x00FFFFFFu) ?
                      word ^ CDBR_XOR : 0u;
    } else if (kind == CDBR_FRAME) {
        if (peer->id && id == peer->id && sequence >= 1u &&
            sequence <= CDBR_PACKETS && epoch(peer) == 0) {
            peer->frame_sequence = sequence;
            peer->count = peer->pending = peer->half_pending = 0u;
            peer->verdict = CDBR_PENDING | CDBR_KEY(id, sequence);
        }
        peer->reply = peer->exhausted ? 0u : word ^ CDBR_XOR;
    } else if (kind == CDBR_QUERY) {
        peer->reply = peer->id && id == peer->id &&
                      sequence == peer->frame_sequence ?
                      peer->verdict : 0u;
    } else {
        peer->reply = word ^ CDBR_XOR;
        if (peer->id && peer->frame_sequence && !peer->pending &&
            peer->count < CDBR_WORDS && !(word & 0x00FF0000u)) {
            if (kind == CDBR_LOW) {
                peer->low_half = word & 0xFFFFu;
                peer->half_pending = 1u;
            } else if (kind == CDBR_HIGH && peer->half_pending) {
                uint32_t raw = ((word & 0xFFFFu) << 16) | peer->low_half;
                peer->half_pending = 0u;
                if (peer->count || raw == CDTP_MAGIC) {
                    peer->words[peer->count++] = raw;
                    if (peer->count == CDBR_WORDS) peer->pending = 1u;
                }
            }
        }
    }
    return peer->reply;
}
int cdbr_peer_take(cdbr_peer *peer, cdbr_work *work)
{
    if (!peer || !work || !peer->pending) return 0;
    work->epoch = peer->epoch;
    work->id = peer->id;
    work->frame_sequence = peer->frame_sequence;
    work->count = peer->count;
    work->accepted = peer->accepted;
    if (peer->frame_sequence == CDBR_PACKETS && peer->accepted == CDBR_PACKETS - 1u)
        memcpy(work->image_prefix, peer->image, sizeof(work->image_prefix));
    memcpy(work->words, peer->words, sizeof(work->words));
    /* Keep pending set: IRQ must not reuse this frame until publish or
     * a new FRAME/RESET invalidates it. */
    return 1;
}
void cdbr_peer_validate(const cdbr_work *work, cdbr_result *result)
{
    cdgu_message message;
    if (!work || !result) return;
    result->epoch = work->epoch;
    result->id = work->id;
    result->sequence = work->frame_sequence;
    result->valid = 0;
    result->image_crc = 0u;
    if (!work->id || work->id > 0xFFFFu || work->count != CDBR_WORDS ||
        work->frame_sequence < 1u || work->frame_sequence > CDBR_PACKETS)
        return;
    if (cdgu_decode(work->words, work->count, CDBR_TRANSFER(work->id), &message) != 0 ||
        message.opcode != 1u || message.sequence != work->frame_sequence ||
        message.payload_length != CDBR_CHUNK_BYTES) return;
    for (unsigned int i = 0; i < CDBR_CHUNK_BYTES; ++i)
        if (message.payload[i] != sample_byte(message.sequence, i)) return;
    memcpy(result->payload, message.payload, sizeof(result->payload));
    if (message.sequence == CDBR_PACKETS && work->accepted == CDBR_PACKETS - 1u) {
        /* Hash the already stored RAM prefix and final decoded chunk outside
         * IRQ exclusion. Publication still requires the same frame epoch. */
        uint32_t crc = image_crc_update(UINT32_MAX, work->image_prefix,
                                        sizeof(work->image_prefix));
        result->image_crc = image_crc_update(crc, message.payload,
                                             CDBR_CHUNK_BYTES) ^ UINT32_MAX;
        if (result->image_crc != CDBR_IMAGE_CRC) return;
    }
    result->valid = 1;
}
int cdbr_peer_publish(cdbr_peer *peer, const cdbr_result *result)
{
    uint32_t key;
    if (!peer || !result || peer->exhausted || !peer->pending ||
        result->epoch != peer->epoch || result->id != peer->id ||
        result->sequence != peer->frame_sequence) return 0;
    peer->pending = 0u;
    key = CDBR_KEY(peer->id, peer->frame_sequence);
    if (!result->valid || result->sequence > peer->accepted + 1u ||
        result->sequence < peer->accepted) {
        peer->verdict = CDBR_NACK | key;
    } else if (result->sequence == peer->accepted) {
        peer->verdict = CDBR_ACK | key; /* duplicate after lost verdict */
    } else if (result->sequence == 2u && !peer->forced_nack) {
        peer->forced_nack = 1u;
        peer->verdict = CDBR_NACK | key;
    } else {
        memcpy(peer->image + peer->accepted * CDBR_CHUNK_BYTES,
               result->payload, CDBR_CHUNK_BYTES);
        ++peer->accepted;
        if (peer->accepted == CDBR_PACKETS) peer->image_crc = result->image_crc;
        peer->verdict = CDBR_ACK | key;
    }
    return 1;
}
