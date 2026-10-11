#include <assert.h>
#include <stdio.h>
#include "../experiments/v2-probe/bulk_packet.h"

#ifndef CUBEDECK_VERDICT_WAIT
#error "Build this test with the separate verdict-wait configuration"
#endif

static void delayed_final(unsigned int delay)
{
    cdbr_tx tx;
    cdbr_peer peer = {0};
    cdbr_work held;
    cdbr_result result;
    unsigned int have_work = 0u, queries = 0u, steps = 0u;
    uint32_t now = UINT32_MAX - 20u, held_epoch = 0u;
    assert(cdbr_tx_init(&tx, 91u) == 0);
    cdbr_peer_reset(&peer);
    while (tx.phase != CDBR_TX_DONE) {
        uint32_t word;
        assert(++steps < 20000u);
        int next = cdbr_tx_next(&tx, now++, &word);
        assert(next >= 0);
        if (!next) continue;
        uint32_t reply = cdbr_peer_word(&peer, word);
        if (tx.sequence == CDBR_PACKETS) {
            if (!have_work && cdbr_peer_take(&peer, &held)) {
                have_work = 1u;
                held_epoch = held.epoch;
                cdbr_peer_validate(&held, &result);
                assert(result.valid && result.image_crc == CDBR_IMAGE_CRC);
            }
            if (tx.phase == CDBR_TX_QUERY &&
                reply == (CDBR_PENDING | CDBR_KEY(tx.id, tx.sequence))) {
                assert(have_work && peer.epoch == held_epoch);
                assert(peer.accepted == CDBR_PACKETS - 1u && peer.image_crc == 0u);
                if (++queries == delay) assert(cdbr_peer_publish(&peer, &result));
            }
        } else {
            cdbr_work work;
            cdbr_result accepted;
            if (cdbr_peer_take(&peer, &work)) {
                cdbr_peer_validate(&work, &accepted);
                assert(cdbr_peer_publish(&peer, &accepted));
            }
        }
        assert(cdbr_tx_reply(&tx, reply, now) >= 0);
    }
    assert(queries == delay && tx.pending_total == delay && tx.pending_polls == 0u);
    assert(tx.recoveries == 1u && tx.last_retry_sequence == 2u);
    assert(tx.last_retry_reason == CDBR_RETRY_NACK);
    assert(peer.accepted == CDBR_PACKETS && peer.image_crc == CDBR_IMAGE_CRC);
    assert(peer.epoch == held_epoch); /* no premature FRAME invalidation */
}

static cdbr_tx query(void)
{
    cdbr_tx tx;
    assert(cdbr_tx_init(&tx, 72u) == 0);
    tx.phase = CDBR_TX_QUERY;
    tx.awaiting = 1u;
    return tx;
}

int main(void)
{
    for (unsigned int delay = 1u; delay < CDBR_PENDING_POLLS; ++delay)
        delayed_final(delay);

    /* Never-ready peer: exactly eight matching pending replies per attempt,
     * two frame retransmissions, then terminal failure. Wrap the timer. */
    cdbr_tx tx = query();
    uint32_t now = UINT32_MAX - 6u;
    for (unsigned int attempt = 0u; attempt <= CDBR_RETRIES; ++attempt) {
        for (unsigned int poll = 1u; poll <= CDBR_PENDING_POLLS; ++poll) {
            int status = cdbr_tx_reply(&tx, CDBR_PENDING | CDBR_KEY(tx.id, tx.sequence), now);
            if (poll < CDBR_PENDING_POLLS) {
                uint32_t word;
                assert(status == 1 && tx.phase == CDBR_TX_GAP && tx.recoveries == attempt);
                assert(cdbr_tx_next(&tx, now + CDBR_GAP - 1u, &word) == 0);
                now += CDBR_GAP;
                assert(cdbr_tx_next(&tx, now, &word) == 1);
                assert(word == (CDBR_QUERY | CDBR_KEY(tx.id, tx.sequence)));
            } else {
                assert(tx.last_retry_reason == CDBR_RETRY_PENDING_LIMIT);
                assert(tx.last_retry_sequence == 1u && tx.pending_polls == 0u);
                assert(status == (attempt == CDBR_RETRIES ? -1 : 1));
                assert(tx.phase == (attempt == CDBR_RETRIES ? CDBR_TX_FAILED : CDBR_TX_FRAME));
            }
        }
        if (attempt < CDBR_RETRIES) {
            tx.phase = CDBR_TX_QUERY;
            tx.awaiting = 1u; /* next completed frame's first query */
        }
    }
    assert(tx.accepted == 0u && tx.recoveries == CDBR_RETRIES);
    assert(tx.pending_total == (CDBR_RETRIES + 1u) * CDBR_PENDING_POLLS);
    uint32_t unrelated[] = {
        CDBR_PENDING | CDBR_KEY(73u, 1u), CDBR_PENDING | CDBR_KEY(72u, 2u),
        CDBR_ACK | CDBR_KEY(73u, 1u), CDBR_NACK | CDBR_KEY(73u, 1u), 0u
    };
    for (unsigned int i = 0u; i < sizeof(unrelated)/sizeof(unrelated[0]); ++i) {
        tx = query();
        assert(cdbr_tx_reply(&tx, unrelated[i], 0u) == 1);
        assert(tx.accepted == 0u && tx.pending_total == 0u && tx.recoveries == 1u);
        assert(tx.last_retry_reason == CDBR_RETRY_VERDICT && tx.last_retry_reply == unrelated[i]);
    }
    tx = query();
    assert(cdbr_tx_reply(&tx, CDBR_NACK | CDBR_KEY(tx.id, 1u), 0u) == 1);
    assert(tx.last_retry_reason == CDBR_RETRY_NACK);
    tx = query();
    tx.phase = CDBR_TX_DATA;
    tx.last_word = CDBR_LOW | 42u;
    assert(cdbr_tx_reply(&tx, 0u, 0u) == 1);
    assert(tx.last_retry_reason == CDBR_RETRY_ECHO && tx.last_retry_reply == 0u);
    assert(cdbr_tx_init(&tx, 74u) == 0);
    assert(!tx.pending_total && !tx.pending_polls && !tx.last_retry_sequence);
    assert(tx.last_retry_reason == CDBR_RETRY_NONE && !tx.last_retry_reply);
    puts("Verdict wait: 7 delayed full-image publications, bounded exhaustion, stale verdicts, diagnostics and wraparound passed.");
    return 0;
}
