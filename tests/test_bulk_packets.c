#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/v2-probe/bulk_packet.h"

static unsigned int cases;
static void process(cdbr_peer *peer)
{
    cdbr_work work;
    cdbr_result result;
    if (cdbr_peer_take(peer, &work)) {
        cdbr_peer_validate(&work, &result);
        assert(cdbr_peer_publish(peer, &result) == 1);
        assert(cdbr_peer_publish(peer, &result) == 0);
    }
}
static void transfer(unsigned int mode, unsigned int position, uint32_t now)
{
    cdbr_tx tx;
    cdbr_peer peer = {0};
    unsigned int fault = 0u, iterations = 0u;
    assert(cdbr_tx_init(&tx, 0x1200u + mode) == 0);
    cdbr_peer_reset(&peer);
    while (tx.phase != CDBR_TX_DONE && tx.phase != CDBR_TX_FAILED) {
        uint32_t word, response, delivered;
        int next;
        assert(++iterations < CDBR_PACKETS * (CDBR_WORDS * 2u + CDBR_GAP + 8u) + 200u);
        next = cdbr_tx_next(&tx, now++, &word);
        assert(next >= 0);
        if (!next) continue;
        assert(cdbr_tx_next(&tx, now, &delivered) == 0); /* stop and wait */
        delivered = word;
        if (!fault && tx.phase == CDBR_TX_DATA && tx.sequence == 1u &&
            tx.index * 2u + tx.high_half == position) {
            if (mode == 1u || mode == 2u) { delivered ^= 1u; fault = 1u; }
            if (mode == 8u) { delivered = 0u; fault = 1u; } /* lost half */
            if (mode == 7u) { cdbr_peer_reset(&peer); fault = 1u; }
        }
        response = cdbr_peer_word(&peer, delivered);
        if ((mode == 2u || mode == 8u) && delivered != word)
            response = word ^ CDBR_XOR; /* damage escapes word echo check */
        if (mode != 6u) process(&peer);
        if (!fault && tx.phase == CDBR_TX_QUERY && tx.sequence == 1u) {
            if (mode == 3u) { response = CDBR_PENDING | CDBR_KEY(tx.id, 1u); fault = 1u; }
            if (mode == 4u) { response = CDBR_ACK | CDBR_KEY(tx.id + 1u, 1u); fault = 1u; }
            if (mode == 5u) { response = CDBR_ACK | CDBR_KEY(tx.id, 2u); fault = 1u; }
        }
        unsigned int accepted = tx.accepted;
        int result = cdbr_tx_reply(&tx, response, now);
        if (mode == 4u || mode == 5u) {
            if (fault && response != (CDBR_ACK | CDBR_KEY(tx.id, accepted + 1u)) &&
                accepted == 0u) assert(tx.accepted == 0u);
        }
        assert(result >= 0 || tx.phase == CDBR_TX_FAILED);
        assert(cdbr_tx_reply(&tx, response, now) ==
               (tx.phase == CDBR_TX_FAILED ? -1 : 0));
    }
    if (mode == 6u || mode == 7u) {
        assert(tx.phase == CDBR_TX_FAILED && tx.accepted == 0u);
        assert(tx.retries == CDBR_RETRIES + 1u);
    } else {
        assert(tx.phase == CDBR_TX_DONE && tx.accepted == CDBR_PACKETS);
        assert(peer.accepted == CDBR_PACKETS && peer.image_crc == CDBR_IMAGE_CRC);
        for (size_t i = 0u; i < CDBR_IMAGE_BYTES; ++i)
            assert(peer.image[i] == cdbr_expected_byte(i));
        assert(tx.recoveries == (mode ? 2u : 1u));
    }
    ++cases;
}
static void capture(cdbr_peer *peer, uint32_t id, unsigned int sequence,
                    const uint32_t *words)
{
    cdbr_peer_word(peer, CDBR_FRAME | CDBR_KEY(id, sequence));
    for (size_t i = 0; i < CDBR_WORDS; ++i) {
        cdbr_peer_word(peer, CDBR_LOW | (words[i] & 0xFFFFu));
        cdbr_peer_word(peer, CDBR_HIGH | (words[i] >> 16));
    }
}
static void worker_and_reset(void)
{
    cdbr_tx tx;
    cdbr_peer peer = {0};
    cdbr_work work;
    cdbr_result result;
    assert(cdbr_tx_init(&tx, 77u) == 0);
    cdbr_peer_reset(&peer);
    cdbr_peer_word(&peer, CDBR_SESSION | tx.id);
    capture(&peer, tx.id, 1u, tx.words);
    assert(cdbr_peer_take(&peer, &work) == 1);
    cdbr_peer_validate(&work, &result);
    assert(result.valid);
    cdbr_peer_reset(&peer);
    assert(cdbr_peer_publish(&peer, &result) == 0);
    assert(peer.accepted == 0u && peer.reply == CDBR_READY && peer.count == 0u);
    cdbr_peer_word(&peer, CDBR_SESSION | 78u);
    /* A stale BEGIN must not replace the new session after RESET negotiation. */
    assert(cdbr_peer_word(&peer, CDBR_SESSION | 77u) == 0u && peer.id == 78u);
    cdbr_peer_word(&peer, CDBR_FRAME | CDBR_KEY(78u, 1u));
    assert(cdbr_peer_publish(&peer, &result) == 0);
    assert(cdbr_peer_word(&peer, CDBR_QUERY | CDBR_KEY(77u, 1u)) == 0u);

    /* New FRAME of the SAME ID invalidates deferred work too. */
    cdbr_peer_reset(&peer);
    cdbr_peer_word(&peer, CDBR_SESSION | tx.id);
    capture(&peer, tx.id, 1u, tx.words);
    assert(cdbr_peer_take(&peer, &work) == 1);
    cdbr_peer_validate(&work, &result);
    cdbr_peer_word(&peer, CDBR_FRAME | CDBR_KEY(tx.id, 1u));
    assert(cdbr_peer_publish(&peer, &result) == 0 && peer.accepted == 0u);

    /* CRC-valid packet with the wrong opcode/payload/ID/sequence is rejected. */
    for (unsigned int fault = 0u; fault < 4u; ++fault) {
        cdgu_message message;
        uint32_t words[CDBR_WORDS];
        size_t count;
        assert(cdgu_decode(tx.words, tx.count, CDBR_TRANSFER(tx.id), &message) == 0);
        switch (fault) {
        case 0: message.opcode = 2u; break;
        case 1: message.payload[0] ^= 1u; break;
        case 2: ++message.session_id; break;
        default: message.sequence = 2u; break;
        }
        assert(cdgu_encode(&message, words, CDBR_WORDS, &count) == 0);
        capture(&peer, tx.id, 1u, words);
        assert(cdbr_peer_take(&peer, &work) == 1);
        cdbr_peer_validate(&work, &result);
        assert(!result.valid && cdbr_peer_publish(&peer, &result) == 1);
        assert(peer.accepted == 0u && peer.verdict == (CDBR_NACK | CDBR_KEY(tx.id, 1u)));
        ++cases;
    }
    for (unsigned int word = 0u; word < CDBR_WORDS; ++word) {
        for (unsigned int bit = 0u; bit < 32u; ++bit) {
            uint32_t words[CDBR_WORDS];
            memcpy(words, tx.words, sizeof(words));
            words[word] ^= UINT32_C(1) << bit;
            capture(&peer, tx.id, 1u, words);
            if (cdbr_peer_take(&peer, &work)) {
                cdbr_peer_validate(&work, &result);
                assert(!result.valid && cdbr_peer_publish(&peer, &result) == 1);
            } else assert(word == 0u); /* invalid magic never starts capture */
            assert(peer.accepted == 0u);
            ++cases;
        }
    }
    /* A well-formed future packet must not skip acceptance order. */
    cdbr_peer_reset(&peer);
    cdbr_peer_word(&peer, CDBR_SESSION | tx.id);
    cdgu_message future;
    uint32_t words[CDBR_WORDS];
    size_t count;
    assert(cdgu_decode(tx.words, tx.count, CDBR_TRANSFER(tx.id), &future) == 0);
    future.sequence = 2u;
    for (unsigned int i = 0u; i < CDBR_CHUNK_BYTES; ++i)
        future.payload[i] = cdbr_expected_byte(CDBR_CHUNK_BYTES + i);
    assert(cdgu_encode(&future, words, CDBR_WORDS, &count) == 0);
    capture(&peer, tx.id, 2u, words);
    assert(cdbr_peer_take(&peer, &work) == 1);
    cdbr_peer_validate(&work, &result);
    assert(result.valid && cdbr_peer_publish(&peer, &result) == 1);
    assert(peer.accepted == 0u && peer.verdict == (CDBR_NACK | CDBR_KEY(tx.id, 2u)));
    ++cases;
    peer.epoch = UINT32_MAX;
    cdbr_peer_reset(&peer);
    assert(peer.exhausted && peer.reply == 0u && !peer.armed);
    assert(cdbr_peer_publish(&peer, &result) == 0);
    ++cases;
}
static void image_integrity(void)
{
    cdbr_peer base = {0};
    cdbr_work work;
    cdbr_result result;
    cdgu_message message;
    uint32_t words[CDBR_WORDS];
    size_t count;
    cdbr_peer_reset(&base);
    cdbr_peer_word(&base, CDBR_SESSION | 99u);
    memset(&message, 0, sizeof(message));
    message.session_id = CDBR_TRANSFER(99u);
    message.opcode = 1u;
    message.payload_length = CDBR_CHUNK_BYTES;
    for (unsigned int seq = 1u; seq <= CDBR_PACKETS; ++seq) {
        message.sequence = (uint16_t)seq;
        for (size_t i = 0u; i < CDBR_CHUNK_BYTES; ++i)
            message.payload[i] = cdbr_expected_byte((seq - 1u) * CDBR_CHUNK_BYTES + i);
        assert(cdgu_encode(&message, words, CDBR_WORDS, &count) == 0);
        assert(count == CDBR_WORDS);
        if (seq == CDBR_PACKETS) break;
        capture(&base, 99u, seq, words);
        process(&base);
        if (seq == 2u) { capture(&base, 99u, seq, words); process(&base); }
        assert(base.accepted == seq && base.image_crc == 0u);
    }
    assert(base.accepted == CDBR_PACKETS - 1u);
    /* Corrupt every byte of the already-stored RAM prefix. Per-frame CRC of
     * the final chunk is valid, but whole-image acceptance must still fail. */
    for (size_t offset = 0u; offset < sizeof(work.image_prefix); ++offset) {
        cdbr_peer damaged = base;
        damaged.image[offset] ^= (uint8_t)(1u << (offset % 8u));
        capture(&damaged, 99u, CDBR_PACKETS, words);
        assert(cdbr_peer_take(&damaged, &work));
        cdbr_peer_validate(&work, &result);
        assert(!result.valid);
        assert(cdbr_peer_publish(&damaged, &result));
        assert(damaged.accepted == CDBR_PACKETS - 1u && damaged.image_crc == 0u);
        assert((damaged.verdict & 0xFF000000u) == CDBR_NACK);
        ++cases;
    }
    cdbr_peer good = base;
    capture(&good, 99u, CDBR_PACKETS, words);
    assert(cdbr_peer_take(&good, &work));
    cdbr_peer_validate(&work, &result);
    assert(result.valid && result.image_crc == CDBR_IMAGE_CRC);
    assert(cdbr_peer_publish(&good, &result));
    assert(good.accepted == CDBR_PACKETS && good.image_crc == CDBR_IMAGE_CRC);
    for (size_t i = 0u; i < CDBR_IMAGE_BYTES; ++i)
        assert(good.image[i] == cdbr_expected_byte(i));
    uint8_t saved[CDBR_IMAGE_BYTES];
    memcpy(saved, good.image, sizeof(saved));
    capture(&good, 99u, CDBR_PACKETS, words); /* lost final ACK: idempotent */
    process(&good);
    assert(good.accepted == CDBR_PACKETS && good.image_crc == CDBR_IMAGE_CRC);
    assert(memcmp(saved, good.image, sizeof(saved)) == 0);
    ++cases;

    capture(&base, 99u, CDBR_PACKETS, words);
    assert(cdbr_peer_take(&base, &work));
    cdbr_peer_validate(&work, &result);
    cdbr_peer_reset(&base); /* late whole-image CRC cannot publish after RESET */
    assert(!cdbr_peer_publish(&base, &result));
    assert(base.accepted == 0u && base.image_crc == 0u);
    ++cases;
}
int main(void)
{
    cdbr_tx tx;
    assert(cdbr_tx_init(&tx, 0u) == -1);
    assert(cdbr_tx_init(&tx, 0x10000u) == -1);
    transfer(0u, 0u, UINT32_MAX - 10u);
    for (unsigned int mode = 1u; mode <= 8u; ++mode) {
        for (unsigned int position = 0u; position < CDBR_WORDS * 2u; ++position)
            transfer(mode, position, position & 1u ? UINT32_MAX - 10u : 0u);
    }
    worker_and_reset();
    image_integrity();
    /* Exercise every usable on-wire ID, including CRC words whose top byte
     * equals a reserved control marker. Tagged halves keep them unambiguous. */
    for (uint32_t id = 1u; id <= 0xFFFFu; ++id) {
        cdbr_peer peer = {0};
        cdbr_work work;
        cdbr_result result;
        assert(cdbr_tx_init(&tx, id) == 0);
        cdbr_peer_reset(&peer);
        cdbr_peer_word(&peer, CDBR_SESSION | id);
        capture(&peer, id, 1u, tx.words);
        assert(cdbr_peer_take(&peer, &work) == 1);
        cdbr_peer_validate(&work, &result);
        assert(result.valid && cdbr_peer_publish(&peer, &result) == 1);
        assert(peer.accepted == 1u);
    }
    puts("All 65535 usable fixture IDs preserve packet/control separation.");
    printf("%u-byte RAM packets: %u corruption, interruption, replay and retry cases passed.\n", CDBR_IMAGE_BYTES, cases);
    return 0;
}
