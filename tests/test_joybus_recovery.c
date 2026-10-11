#include "cubedeck/joybus_recovery.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static uint32_t issue(cdjr_link *l, uint32_t now, cdjr_action expected)
{
    cdjr_action action = CDJR_NONE;
    uint32_t token = 0u;
    assert(cdjr_poll(l, now, &action, &token) == 1);
    assert(action == expected && token != 0u);
    return token;
}

int main(void)
{
    cdjr_link l;
    cdjr_action action;
    uint32_t token, old;
    unsigned int i;
    assert(cdjr_init(&l, 120u) == 0);
    assert(cdjr_begin(&l, 0u) == 0);
    token = issue(&l, 10u, CDJR_SEND_RESET);
    assert(cdjr_begin(&l, 10u) == -1);
    assert(cdjr_callback(&l, token + 1u, 0u) == 0);
    assert(l.phase == CDJR_RESET && l.inflight);
    assert(cdjr_poll(&l, 129u, &action, &old) == 0);
    assert(cdjr_callback(&l, token, 0u) == 1);
    old = issue(&l, 130u, CDJR_SEND_STATUS);
    assert(old != token);
    assert(cdjr_callback(&l, token, 0u) == 0);
    assert(cdjr_callback(&l, old, 0u) == 1);
    assert(l.phase == CDJR_READY);
    assert(cdjr_poll(&l, 140u, &action, &token) == 0);
    assert(cdjr_begin(&l, 150u) == 0);
    token = issue(&l, 151u, CDJR_SEND_RESET);
    assert(token != old);
    assert(cdjr_callback(&l, old, 0u) == 0);
    assert(cdjr_callback(&l, token, 8u) == -1);
    assert(l.phase == CDJR_FAILED && l.error == CDJR_ERR_CALLBACK);
    assert(l.last_callback_result == 8u);
    assert(cdjr_begin(&l, 160u) == 0);
    token = issue(&l, 161u, CDJR_SEND_RESET);
    assert(cdjr_poll(&l, 281u, &action, &old) == -1);
    assert(l.error == CDJR_ERR_TIMEOUT && l.phase == CDJR_FAILED);
    assert(l.inflight == 1u);
    assert(cdjr_callback(&l, token, 0u) == 0);
    assert(cdjr_begin(&l, 300u) == -1);
    /* A timeout requires full external SI quiescence / power-cycle:
     * the controller itself must not resubmit a busy transfer. */
    for (i = 0; i < 8u; ++i)
        assert(cdjr_poll(&l, (uint32_t)(301u + i), &action, &old) == -1);
    assert(cdjr_init(&l, 5u) == 0);
    assert(cdjr_begin(&l, UINT32_MAX - 4u) == 0);
    token = issue(&l, UINT32_MAX - 2u, CDJR_SEND_RESET);
    assert(cdjr_poll(&l, 0u, &action, &old) == 0);
    assert(cdjr_poll(&l, 2u, &action, &old) == -1);
    assert(l.error == CDJR_ERR_TIMEOUT);
    /* Configurable quiet period and interruption boundaries. */
    assert(cdjr_init(&l, 50u) == 0);
    assert(cdjr_set_settle(&l, 4u) == 0);
    assert(cdjr_begin(&l, 5u) == 0);
    token = issue(&l, 5u, CDJR_SEND_RESET);
    assert(cdjr_callback(&l, token, 0u) == 1);
    assert(cdjr_poll(&l, 6u, &action, &old) == 0);
    assert(cdjr_poll(&l, 9u, &action, &old) == 0);
    old = issue(&l, 10u, CDJR_SEND_STATUS);
    assert(cdjr_interrupt(&l) == -1);
    assert(l.inflight && l.error == CDJR_ERR_TIMEOUT);
    assert(cdjr_callback(&l, old, 0u) == 0);
    assert(cdjr_begin(&l, 11u) == -1);
    assert(cdjr_init(&l, 50u) == 0);
    assert(cdjr_begin(&l, 0u) == 0);
    assert(cdjr_interrupt(&l) == 0);
    assert(cdjr_begin(&l, 1u) == 0);
    token = issue(&l, 2u, CDJR_SEND_RESET);
    assert(cdjr_callback(&l, token, 0u) == 1);
    assert(cdjr_interrupt(&l) == 0);
    assert(cdjr_begin(&l, 3u) == 0);
    assert(cdjr_callback(&l, token, 0u) == 0);
    puts("Joybus startup recovery and stale callback fault matrix passed.");
    return 0;
}
