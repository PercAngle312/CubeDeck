#include "cubedeck/joybus_recovery.h"
#include <string.h>

int cdjr_init(cdjr_link *link, uint32_t timeout_ticks)
{
    if (!link || !timeout_ticks || timeout_ticks >= 0x80000000u)
        return -1;
    memset(link, 0, sizeof(*link));
    link->timeout_ticks = timeout_ticks;
    return 0;
}

int cdjr_begin(cdjr_link *link, uint32_t now)
{
    (void)now;
    if (!link || !link->timeout_ticks || link->inflight ||
        (link->phase != CDJR_IDLE && link->phase != CDJR_READY &&
         link->phase != CDJR_FAILED))
        return -1;
    /* Tokens stay monotonic across application sessions. Callers must
     * still ensure no physical SI transfer is in flight before begin(). */
    if (link->error == CDJR_ERR_TIMEOUT)
        return -1;
    ++link->generation;
    if (!link->generation)
        ++link->generation;
    link->next_action = CDJR_SEND_RESET;
    link->phase = CDJR_RESET;
    link->error = CDJR_ERR_NONE;
    link->last_callback_result = 0u;
    return 0;
}

static int fail(cdjr_link *link, cdjr_error error)
{
    link->error = error;
    link->phase = CDJR_FAILED;
    link->next_action = 0u;
    return -1;
}

int cdjr_poll(cdjr_link *link, uint32_t now, cdjr_action *action,
              uint32_t *token)
{
    if (!link || !action || !token)
        return -1;
    *action = CDJR_NONE;
    *token = 0u;
    if (link->inflight) {
        if ((uint32_t)(now - link->started_tick) >= link->timeout_ticks)
            return fail(link, CDJR_ERR_TIMEOUT);
        return 0;
    }
    if (link->phase == CDJR_FAILED)
        return -1;
    if (link->next_action) {
        *action = (cdjr_action)link->next_action;
        link->next_action = 0u;
        ++link->active_token;
        if (!link->active_token)
            ++link->active_token;
        *token = link->active_token;
        link->inflight = 1u;
        link->started_tick = now;
        return 1;
    }
    return 0;
}

int cdjr_callback(cdjr_link *link, uint32_t token, uint32_t result)
{
    if (!link || !token)
        return -1;
    /* Tokens are monotonic across sessions. A new session cannot be
     * initiated until the previous hardware call is quiesced. */
    if (!link->inflight || token != link->active_token ||
        link->phase == CDJR_FAILED)
        return 0;
    link->inflight = 0u;
    link->last_callback_result = result;
    if (result != 0u)
        return fail(link, CDJR_ERR_CALLBACK);
    if (link->phase == CDJR_RESET) {
        link->phase = CDJR_STATUS;
        link->next_action = CDJR_SEND_STATUS;
        return 1;
    }
    if (link->phase == CDJR_STATUS) {
        link->phase = CDJR_READY;
        link->next_action = 0u;
        return 1;
    }
    return fail(link, CDJR_ERR_BAD_PHASE);
}
