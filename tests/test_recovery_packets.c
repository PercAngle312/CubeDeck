#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/v2-probe/recovery_packet.h"

static unsigned int cases;
static void process(cdrp_peer *peer)
{
    cdrp_work work;
    cdrp_result result;
    if (cdrp_peer_take(peer, &work)) {
        cdrp_peer_validate(&work, &result);
        assert(cdrp_peer_publish(peer, &result) == 1);
        assert(cdrp_peer_publish(peer, &result) == 0);
    }
}
static void transfer(unsigned int mode, unsigned int position, uint32_t now)
{
    cdrp_tx tx;
    cdrp_peer peer = {0};
    unsigned int fault = 0u, iterations = 0u;
    assert(cdrp_tx_init(&tx, 0x1200u + mode) == 0);
    cdrp_peer_reset(&peer);
    while (tx.phase != CDRP_TX_DONE && tx.phase != CDRP_TX_FAILED) {
        uint32_t word, response, delivered;
        int next;
        assert(++iterations < 2000u);
        next = cdrp_tx_next(&tx, now++, &word);
        assert(next >= 0);
        if (!next) continue;
        assert(cdrp_tx_next(&tx, now, &delivered) == 0); /* stop and wait */
        delivered = word;
        if (!fault && tx.phase == CDRP_TX_DATA && tx.sequence == 1u &&
            tx.index * 2u + tx.high_half == position) {
            if (mode == 1u || mode == 2u) { delivered ^= 1u; fault = 1u; }
            if (mode == 8u) { delivered = 0u; fault = 1u; } /* lost half */
            if (mode == 7u) { cdrp_peer_reset(&peer); fault = 1u; }
        }
        response = cdrp_peer_word(&peer, delivered);
        if ((mode == 2u || mode == 8u) && delivered != word)
            response = word ^ CDRP_XOR; /* damage escapes word echo check */
        if (mode != 6u) process(&peer);
        if (!fault && tx.phase == CDRP_TX_QUERY && tx.sequence == 1u) {
            if (mode == 3u) { response = CDRP_PENDING | CDRP_KEY(tx.id, 1u); fault = 1u; }
            if (mode == 4u) { response = CDRP_ACK | CDRP_KEY(tx.id + 1u, 1u); fault = 1u; }
            if (mode == 5u) { response = CDRP_ACK | CDRP_KEY(tx.id, 2u); fault = 1u; }
        }
        unsigned int accepted = tx.accepted;
        int result = cdrp_tx_reply(&tx, response, now);
        if (mode == 4u || mode == 5u) {
            if (fault && response != (CDRP_ACK | CDRP_KEY(tx.id, accepted + 1u)) &&
                accepted == 0u) assert(tx.accepted == 0u);
        }
        assert(result >= 0 || tx.phase == CDRP_TX_FAILED);
        assert(cdrp_tx_reply(&tx, response, now) ==
               (tx.phase == CDRP_TX_FAILED ? -1 : 0));
    }
    if (mode == 6u || mode == 7u) {
        assert(tx.phase == CDRP_TX_FAILED && tx.accepted == 0u);
        assert(tx.retries == CDRP_RETRIES + 1u);
    } else {
        assert(tx.phase == CDRP_TX_DONE && tx.accepted == 3u);
        assert(peer.accepted == 3u);
        assert(tx.recoveries == (mode ? 2u : 1u));
    }
    ++cases;
}
static void capture(cdrp_peer *peer, uint32_t id, unsigned int sequence,
                    const uint32_t *words)
{
    cdrp_peer_word(peer, CDRP_FRAME | CDRP_KEY(id, sequence));
    for (size_t i = 0; i < CDRP_WORDS; ++i) {
        cdrp_peer_word(peer, CDRP_LOW | (words[i] & 0xFFFFu));
        cdrp_peer_word(peer, CDRP_HIGH | (words[i] >> 16));
    }
}
static void worker_and_reset(void)
{
    cdrp_tx tx;
    cdrp_peer peer = {0};
    cdrp_work work;
    cdrp_result result;
    assert(cdrp_tx_init(&tx, 77u) == 0);
    cdrp_peer_reset(&peer);
    cdrp_peer_word(&peer, CDRP_SESSION | tx.id);
    capture(&peer, tx.id, 1u, tx.words);
    assert(cdrp_peer_take(&peer, &work) == 1);
    cdrp_peer_validate(&work, &result);
    assert(result.valid);
    cdrp_peer_reset(&peer);
    assert(cdrp_peer_publish(&peer, &result) == 0);
    assert(peer.accepted == 0u && peer.reply == CDRP_READY && peer.count == 0u);
    cdrp_peer_word(&peer, CDRP_SESSION | 78u);
    /* A stale BEGIN must not replace the new session after RESET negotiation. */
    assert(cdrp_peer_word(&peer, CDRP_SESSION | 77u) == 0u && peer.id == 78u);
    cdrp_peer_word(&peer, CDRP_FRAME | CDRP_KEY(78u, 1u));
    assert(cdrp_peer_publish(&peer, &result) == 0);
    assert(cdrp_peer_word(&peer, CDRP_QUERY | CDRP_KEY(77u, 1u)) == 0u);

    /* New FRAME of the SAME ID invalidates deferred work too. */
    cdrp_peer_reset(&peer);
    cdrp_peer_word(&peer, CDRP_SESSION | tx.id);
    capture(&peer, tx.id, 1u, tx.words);
    assert(cdrp_peer_take(&peer, &work) == 1);
    cdrp_peer_validate(&work, &result);
    cdrp_peer_word(&peer, CDRP_FRAME | CDRP_KEY(tx.id, 1u));
    assert(cdrp_peer_publish(&peer, &result) == 0 && peer.accepted == 0u);

    /* CRC-valid packet with the wrong opcode/payload/ID/sequence is rejected. */
    for (unsigned int fault = 0u; fault < 4u; ++fault) {
        cdgu_message message;
        uint32_t words[CDRP_WORDS];
        size_t count;
        assert(cdgu_decode(tx.words, tx.count, CDRP_TRANSFER(tx.id), &message) == 0);
        switch (fault) {
        case 0: message.opcode = 2u; break;
        case 1: message.payload[0] ^= 1u; break;
        case 2: ++message.session_id; break;
        default: message.sequence = 2u; break;
        }
        assert(cdgu_encode(&message, words, CDRP_WORDS, &count) == 0);
        capture(&peer, tx.id, 1u, words);
        assert(cdrp_peer_take(&peer, &work) == 1);
        cdrp_peer_validate(&work, &result);
        assert(!result.valid && cdrp_peer_publish(&peer, &result) == 1);
        assert(peer.accepted == 0u && peer.verdict == (CDRP_NACK | CDRP_KEY(tx.id, 1u)));
        ++cases;
    }
    for (unsigned int word = 0u; word < CDRP_WORDS; ++word) {
        for (unsigned int bit = 0u; bit < 32u; ++bit) {
            uint32_t words[CDRP_WORDS];
            memcpy(words, tx.words, sizeof(words));
            words[word] ^= UINT32_C(1) << bit;
            capture(&peer, tx.id, 1u, words);
            if (cdrp_peer_take(&peer, &work)) {
                cdrp_peer_validate(&work, &result);
                assert(!result.valid && cdrp_peer_publish(&peer, &result) == 1);
            } else assert(word == 0u); /* invalid magic never starts capture */
            assert(peer.accepted == 0u);
            ++cases;
        }
    }
    /* A well-formed future packet must not skip acceptance order. */
    cdrp_peer_reset(&peer);
    cdrp_peer_word(&peer, CDRP_SESSION | tx.id);
    cdgu_message future;
    uint32_t words[CDRP_WORDS];
    size_t count;
    assert(cdgu_decode(tx.words, tx.count, CDRP_TRANSFER(tx.id), &future) == 0);
    future.sequence = 2u;
    for (unsigned int i = 0u; i < 7u; ++i) future.payload[i] ^= 17u;
    assert(cdgu_encode(&future, words, CDRP_WORDS, &count) == 0);
    capture(&peer, tx.id, 2u, words);
    assert(cdrp_peer_take(&peer, &work) == 1);
    cdrp_peer_validate(&work, &result);
    assert(result.valid && cdrp_peer_publish(&peer, &result) == 1);
    assert(peer.accepted == 0u && peer.verdict == (CDRP_NACK | CDRP_KEY(tx.id, 2u)));
    ++cases;
    peer.epoch = UINT32_MAX;
    cdrp_peer_reset(&peer);
    assert(peer.exhausted && peer.reply == 0u && !peer.armed);
    assert(cdrp_peer_publish(&peer, &result) == 0);
    ++cases;
}
int main(void)
{
    cdrp_tx tx;
    assert(cdrp_tx_init(&tx, 0u) == -1);
    assert(cdrp_tx_init(&tx, 0x10000u) == -1);
    transfer(0u, 0u, UINT32_MAX - 10u);
    for (unsigned int mode = 1u; mode <= 8u; ++mode) {
        for (unsigned int position = 0u; position < CDRP_WORDS * 2u; ++position)
            transfer(mode, position, position & 1u ? UINT32_MAX - 10u : 0u);
    }
    worker_and_reset();
    /* Exercise every usable on-wire ID, including CRC words whose top byte
     * equals a reserved control marker. Tagged halves keep them unambiguous. */
    for (uint32_t id = 1u; id <= 0xFFFFu; ++id) {
        cdrp_peer peer = {0};
        cdrp_work work;
        cdrp_result result;
        assert(cdrp_tx_init(&tx, id) == 0);
        cdrp_peer_reset(&peer);
        cdrp_peer_word(&peer, CDRP_SESSION | id);
        capture(&peer, id, 1u, tx.words);
        assert(cdrp_peer_take(&peer, &work) == 1);
        cdrp_peer_validate(&work, &result);
        assert(result.valid && cdrp_peer_publish(&peer, &result) == 1);
        assert(peer.accepted == 1u);
    }
    puts("All 65535 usable fixture IDs preserve packet/control separation.");
    printf("Recovery packets: %u corruption, interruption, replay and retry cases passed.\n", cases);
    return 0;
}
