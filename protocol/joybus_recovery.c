#include "cubedeck/joybus_recovery.h"
#include <string.h>

static int fail(cdjr_link *link, cdjr_error error)
{
    link->error = error;
    link->phase = CDJR_FAILED;
    link->next_action = 0u;
    link->waiting = 0u;
    return -1;
}

int cdjr_init(cdjr_link *link, uint32_t timeout_ticks)
{
    if (!link || !timeout_ticks || timeout_ticks >= 0x80000000u)
        return -1;
    memset(link, 0, sizeof(*link));
    link->timeout_ticks = timeout_ticks;
    return 0;
}

int cdjr_configure(cdjr_link *link, uint32_t settle_ticks,
                   unsigned int max_sync_attempts)
{
    if (!link || !link->timeout_ticks || link->phase != CDJR_IDLE ||
        settle_ticks >= 0x80000000u)
        return -1;
    link->settle_ticks = settle_ticks;
    link->max_sync_attempts = max_sync_attempts;
    return 0;
}

int cdjr_begin(cdjr_link *link, uint32_t now)
{
    if (!link || !link->timeout_ticks || link->inflight ||
        (link->phase != CDJR_IDLE && link->phase != CDJR_READY &&
         link->phase != CDJR_FAILED))
        return -1;
    if (link->error == CDJR_ERR_TIMEOUT ||
        link->error == CDJR_ERR_TOKEN_EXHAUSTED)
        return -1;
    ++link->generation;
    if (!link->generation) ++link->generation;
    link->next_action = CDJR_SEND_RESET;
    link->phase = CDJR_RESET;
    link->error = CDJR_ERR_NONE;
    link->last_callback_result = 0u;
    link->sync_attempts = 0u;
    link->waiting = 0u;
    link->wait_tick = now;
    return 0;
}

int cdjr_poll(cdjr_link *link, uint32_t now, cdjr_action *action,
              uint32_t *token)
{
    if (!action || !token) return -1;
    *action = CDJR_NONE;
    *token = 0u;
    if (!link || !link->timeout_ticks) return -1;
    /* An interrupted pending transfer still needs its watchdog. */
    if (link->inflight &&
        (uint32_t)(now - link->started_tick) >= link->timeout_ticks)
        return fail(link, CDJR_ERR_TIMEOUT);
    if (link->phase == CDJR_FAILED) return -1;
    if (link->inflight) return 0;
    if (link->waiting &&
        (uint32_t)(now - link->wait_tick) < link->settle_ticks)
        return 0;
    if (!link->next_action) return 0;
    /* Refuse aliasing even a very old token after UINT32_MAX. */
    if (link->active_token == UINT32_MAX)
        return fail(link, CDJR_ERR_TOKEN_EXHAUSTED);
    *action = (cdjr_action)link->next_action;
    link->next_action = 0u;
    link->waiting = 0u;
    *token = ++link->active_token;
    link->inflight = 1u;
    link->started_tick = now;
    if (link->phase == CDJR_SYNC) ++link->sync_attempts;
    return 1;
}

static int queue(cdjr_link *link, cdjr_phase phase, cdjr_action action,
                 uint32_t now)
{
    link->phase = phase;
    link->next_action = (unsigned int)action;
    link->waiting = 1u;
    link->wait_tick = now;
    return 1;
}

int cdjr_complete(cdjr_link *link, uint32_t token, uint32_t result,
                   int peer_ready, uint32_t now)
{
    if (!link || !token) return -1;
    if (!link->inflight || token != link->active_token) return 0;
    /* Matching late completion releases ownership but never revives a failed
     * session. Timeout stays latched; interrupted sessions can restart. */
    link->inflight = 0u;
    link->last_callback_result = result;
    if (link->phase == CDJR_FAILED) {
        if (link->error == CDJR_ERR_INTERRUPTED &&
            (uint32_t)(now - link->started_tick) >= link->timeout_ticks)
            fail(link, CDJR_ERR_TIMEOUT);
        return 0;
    }
    /* A completion at/after the deadline cannot bypass the watchdog simply
     * because the owner processed its mailbox before calling poll(). */
    if ((uint32_t)(now - link->started_tick) >= link->timeout_ticks)
        return fail(link, CDJR_ERR_TIMEOUT);
    if (result != 0u) return fail(link, CDJR_ERR_CALLBACK);
    if (link->phase == CDJR_RESET)
        return queue(link, CDJR_STATUS, CDJR_SEND_STATUS, now);
    if (link->phase == CDJR_STATUS && link->max_sync_attempts)
        return queue(link, CDJR_SYNC, CDJR_READ_READY, now);
    if (link->phase == CDJR_SYNC && !peer_ready) {
        if (link->sync_attempts >= link->max_sync_attempts)
            return fail(link, CDJR_ERR_NOT_READY);
        return queue(link, CDJR_SYNC, CDJR_READ_READY, now);
    }
    if (link->phase == CDJR_STATUS || link->phase == CDJR_SYNC) {
        link->phase = CDJR_READY;
        link->next_action = 0u;
        return 1;
    }
    return fail(link, CDJR_ERR_BAD_PHASE);
}

int cdjr_callback(cdjr_link *link, uint32_t token, uint32_t result)
{
    if (!link) return -1;
    return cdjr_complete(link, token, result, 1, link->started_tick);
}

int cdjr_rejected(cdjr_link *link, uint32_t token)
{
    if (!link || !token) return -1;
    if (!link->inflight || token != link->active_token) return 0;
    link->inflight = 0u;
    if (link->phase == CDJR_FAILED) return 0;
    return fail(link, CDJR_ERR_SUBMIT);
}

int cdjr_interrupt(cdjr_link *link)
{
    if (!link) return -1;
    if (link->phase == CDJR_FAILED) return 0;
    return fail(link, CDJR_ERR_INTERRUPTED);
}
