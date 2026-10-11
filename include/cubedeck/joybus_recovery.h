#ifndef CUBEDECK_JOYBUS_RECOVERY_H
#define CUBEDECK_JOYBUS_RECOVERY_H

#include <stdint.h>

/* Portable stop-and-wait startup/session controller. No SI calls and no
 * storage operations. The owner issues only the command returned by poll(),
 * then reports its asynchronous callback. A stalled SI call is terminal:
 * do not recycle its command buffers until hardware is reset/known idle.
 */
typedef enum {
    CDJR_IDLE = 0, CDJR_RESET, CDJR_STATUS, CDJR_READY, CDJR_FAILED
} cdjr_phase;

typedef enum {
    CDJR_NONE = 0, CDJR_SEND_RESET = 1, CDJR_SEND_STATUS = 2,
    CDJR_START_DATA = 3, CDJR_ABORT = -1
} cdjr_action;

typedef enum {
    CDJR_ERR_NONE = 0, CDJR_ERR_ARG = 1, CDJR_ERR_CALLBACK = 2,
    CDJR_ERR_TIMEOUT = 3, CDJR_ERR_STALE = 4,
    CDJR_ERR_BAD_PHASE = 5
} cdjr_error;

typedef struct {
    cdjr_phase phase;
    cdjr_error error;
    uint32_t generation;
    uint32_t active_token;
    uint32_t started_tick;
    uint32_t last_callback_result;
    uint32_t timeout_ticks;
    uint32_t settle_ticks;
    uint32_t ready_tick;
    unsigned int settling;
    unsigned int inflight;
    unsigned int next_action;
} cdjr_link;

/* Generation starts nonzero, increments whenever the application starts
 * a new software session. Token zero is never issued. Tick math is modular.
 */
int cdjr_init(cdjr_link *link, uint32_t timeout_ticks);
/* Optional delay between completed RESET and STATUS; default is zero. */
int cdjr_set_settle(cdjr_link *link, uint32_t ticks);
/* Interrupt is safe only when no physical transfer is outstanding. An
 * outstanding transfer fails closed and must not be restarted. */
int cdjr_interrupt(cdjr_link *link);
int cdjr_begin(cdjr_link *link, uint32_t now);
/* 1: command available, 0: none, -1: failed. Returned token is the exact
 * value the asynchronous callback must present to cdjr_callback().
 */
int cdjr_poll(cdjr_link *link, uint32_t now, cdjr_action *action,
              uint32_t *token);
/* 1: advanced, 0: ignored delayed callback, -1: callback failed */
int cdjr_callback(cdjr_link *link, uint32_t token, uint32_t result);
#endif
