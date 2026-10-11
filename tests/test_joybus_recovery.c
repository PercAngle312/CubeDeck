#include "cubedeck/joybus_recovery.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static unsigned int scenarios;
static uint32_t issue(cdjr_link *l, uint32_t now, cdjr_action expected)
{
    cdjr_action action = CDJR_NONE;
    uint32_t token = 0u;
    assert(cdjr_poll(l, now, &action, &token) == 1);
    assert(action == expected && token != 0u && l->inflight);
    return token;
}
static void none(cdjr_link *l, uint32_t now, int expected)
{
    cdjr_action action = CDJR_ABORT;
    uint32_t token = 99u;
    assert(cdjr_poll(l, now, &action, &token) == expected);
    assert(action == CDJR_NONE && token == 0u);
}
static void init(cdjr_link *l, uint32_t now)
{
    assert(cdjr_init(l, 10u) == 0);
    assert(cdjr_configure(l, 2u, 3u) == 0);
    assert(cdjr_begin(l, now) == 0);
}
static uint32_t reach(cdjr_link *l, unsigned int phase, uint32_t base)
{
    uint32_t token;
    unsigned int i;
    static const cdjr_action actions[] = {
        CDJR_SEND_RESET, CDJR_SEND_STATUS, CDJR_READ_READY
    };
    init(l, base);
    for (i = 0u; i <= phase; ++i) {
        token = issue(l, base + i * 3u, actions[i]);
        if (i != phase)
            assert(cdjr_complete(l, token, 0u, 0, base + i * 3u + 1u) == 1);
    }
    return token;
}
static void phase_faults(void)
{
    cdjr_link l;
    unsigned int phase, fault;
    uint32_t token, old, base;
    for (phase = 0; phase < 3u; ++phase) {
        for (fault = 0; fault < 8u; ++fault) {
            token = reach(&l, phase, 100u);
            base = 100u + phase * 3u;
            assert(cdjr_begin(&l, base) == -1);
            assert(cdjr_configure(&l, 0, 0) == -1);
            assert(cdjr_complete(&l, token + 1u, 0u, 1, base + 1u) == 0);
            assert(l.inflight);
            none(&l, base + 9u, 0);
            switch (fault) {
            case 0: /* callback error: drained, explicit restart permitted */
                assert(cdjr_complete(&l, token, 8u, 1, base + 9u) == -1);
                assert(l.error == CDJR_ERR_CALLBACK && !l.inflight);
                assert(l.last_callback_result == 8u);
                break;
            case 1: /* transfer never accepted */
                assert(cdjr_rejected(&l, token + 1u) == 0);
                assert(l.inflight);
                assert(cdjr_rejected(&l, token) == -1);
                assert(l.error == CDJR_ERR_SUBMIT && !l.inflight);
                break;
            case 2: /* interrupted while accepted: wait for exact callback */
                assert(cdjr_interrupt(&l) == -1);
                assert(cdjr_begin(&l, base + 1u) == -1);
                none(&l, base + 9u, -1);
                assert(cdjr_complete(&l, token + 1u, 0u, 1, base + 9u) == 0);
                assert(l.inflight);
                assert(cdjr_complete(&l, token, 0u, 1, base + 9u) == 0);
                assert(l.error == CDJR_ERR_INTERRUPTED && !l.inflight);
                break;
            case 3: /* watchdog, then a late successful callback */
                none(&l, base + 10u, -1);
                assert(l.error == CDJR_ERR_TIMEOUT && l.inflight);
                assert(cdjr_complete(&l, token, 0u, 1, base + 11u) == 0);
                assert(!l.inflight && l.error == CDJR_ERR_TIMEOUT);
                assert(cdjr_begin(&l, base + 12u) == -1);
                ++scenarios;
                continue;
            case 4: /* callback arrives at deadline before poll */
                assert(cdjr_complete(&l, token, 0u, 1, base + 10u) == -1);
                assert(l.error == CDJR_ERR_TIMEOUT && !l.inflight);
                assert(cdjr_begin(&l, base + 11u) == -1);
                ++scenarios;
                continue;
            case 5: /* interrupted but callback never comes */
                assert(cdjr_interrupt(&l) == -1);
                none(&l, base + 10u, -1);
                assert(l.error == CDJR_ERR_TIMEOUT && l.inflight);
                assert(cdjr_begin(&l, base + 11u) == -1);
                ++scenarios;
                continue;
            case 6: /* interrupted after completion, before next command */
                assert(cdjr_complete(&l, token, 0u, 1, base + 1u) == 1);
                assert(cdjr_interrupt(&l) == -1);
                assert(!l.inflight);
                break;
            default: /* duplicate completion never advances a second phase */
                assert(cdjr_complete(&l, token, 0u, 1, base + 1u) == 1);
                assert(cdjr_complete(&l, token, 8u, 1, base + 2u) == 0);
                assert(cdjr_interrupt(&l) == -1);
                break;
            }
            none(&l, base + 12u, -1);
            assert(cdjr_begin(&l, base + 13u) == 0);
            old = issue(&l, base + 13u, CDJR_SEND_RESET);
            assert(old > token);
            assert(cdjr_complete(&l, token, 0u, 1, base + 14u) == 0);
            assert(l.phase == CDJR_RESET && l.inflight);
            ++scenarios;
        }
    }
}
static void synchronization(void)
{
    cdjr_link l;
    uint32_t t, now;
    unsigned int ready_on, attempt, wrapped;
    for (wrapped = 0u; wrapped < 2u; ++wrapped) {
        for (ready_on = 0u; ready_on <= 3u; ++ready_on) {
            now = wrapped ? UINT32_MAX - 5u : 0u;
            t = reach(&l, 0u, now);
            assert(cdjr_complete(&l, t, 0u, 0, now + 1u) == 1);
            none(&l, now + 2u, 0);
            t = issue(&l, now + 3u, CDJR_SEND_STATUS);
            assert(cdjr_complete(&l, t, 0u, 0, now + 4u) == 1);
            assert(l.phase == CDJR_SYNC); /* STATUS alone is insufficient */
            none(&l, now + 5u, 0);
            now += 6u;
            for (attempt = 1u; attempt <= 3u; ++attempt) {
                t = issue(&l, now, CDJR_READ_READY);
                if (attempt == ready_on) {
                    assert(cdjr_complete(&l, t, 0u, 1, now + 1u) == 1);
                    assert(l.phase == CDJR_READY);
                    none(&l, now + 2u, 0);
                    break;
                }
                assert(cdjr_complete(&l, t, 0u, 0, now + 1u) ==
                       (attempt == 3u ? -1 : 1));
                if (attempt == 3u) {
                    assert(l.error == CDJR_ERR_NOT_READY && !l.inflight);
                    assert(cdjr_begin(&l, now + 2u) == 0);
                } else none(&l, now + 2u, 0);
                now += 3u;
            }
            ++scenarios;
        }
    }
}
int main(void)
{
    cdjr_link l;
    uint32_t token;
    assert(cdjr_init(NULL, 1u) == -1);
    assert(cdjr_init(&l, 0u) == -1);
    assert(cdjr_init(&l, 0x80000000u) == -1);
    assert(cdjr_init(&l, 10u) == 0);
    assert(cdjr_configure(&l, 0x80000000u, 1u) == -1);
    /* Legacy RESET/STATUS users remain supported. */
    assert(cdjr_begin(&l, 0u) == 0);
    token = issue(&l, 0u, CDJR_SEND_RESET);
    assert(cdjr_callback(&l, token, 0u) == 1);
    token = issue(&l, 1u, CDJR_SEND_STATUS);
    assert(cdjr_callback(&l, token, 0u) == 1);
    assert(l.phase == CDJR_READY);
    assert(cdjr_begin(&l, 2u) == 0);
    l.active_token = UINT32_MAX - 1u;
    token = issue(&l, 2u, CDJR_SEND_RESET);
    assert(token == UINT32_MAX);
    assert(cdjr_complete(&l, token, 0u, 1, 3u) == 1);
    none(&l, 3u, -1);
    assert(l.error == CDJR_ERR_TOKEN_EXHAUSTED && !l.inflight);
    assert(cdjr_begin(&l, 4u) == -1);
    /* Watchdog deadline and interruption drain across tick wrap. */
    init(&l, UINT32_MAX - 4u);
    token = issue(&l, UINT32_MAX - 4u, CDJR_SEND_RESET);
    none(&l, 4u, 0);
    assert(cdjr_interrupt(&l) == -1);
    assert(cdjr_complete(&l, token, 0u, 1, 5u) == 0);
    assert(l.error == CDJR_ERR_TIMEOUT && !l.inflight);
    assert(cdjr_begin(&l, 6u) == -1);
    phase_faults();
    synchronization();
    printf("Joybus startup/recovery: %u fault/synchronization scenarios passed.\n",
           scenarios);
    return 0;
}
