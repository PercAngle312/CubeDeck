#ifndef CUBEDECK_JOYBUS_RECOVERY_H
#define CUBEDECK_JOYBUS_RECOVERY_H
#include <stdint.h>

/* One owner, one outstanding SI command. Never recycle buffers while
 * inflight is set. Timeouts remain terminal, even after a late callback.
 * All elapsed-time intervals must be observed within one 32-bit tick wrap. */
typedef enum {
    CDJR_IDLE = 0, CDJR_RESET, CDJR_STATUS, CDJR_READY, CDJR_FAILED,
    CDJR_SYNC
} cdjr_phase;
typedef enum {
    CDJR_NONE = 0, CDJR_SEND_RESET = 1, CDJR_SEND_STATUS = 2,
    CDJR_START_DATA = 3, CDJR_READ_READY = 4, CDJR_ABORT = -1
} cdjr_action;
typedef enum {
    CDJR_ERR_NONE = 0, CDJR_ERR_ARG = 1, CDJR_ERR_CALLBACK = 2,
    CDJR_ERR_TIMEOUT = 3, CDJR_ERR_STALE = 4, CDJR_ERR_BAD_PHASE = 5,
    CDJR_ERR_NOT_READY = 6, CDJR_ERR_INTERRUPTED = 7,
    CDJR_ERR_SUBMIT = 8, CDJR_ERR_TOKEN_EXHAUSTED = 9
} cdjr_error;
typedef struct {
    cdjr_phase phase;
    cdjr_error error;
    uint32_t generation, active_token, started_tick, last_callback_result;
    uint32_t timeout_ticks, settle_ticks, wait_tick;
    unsigned int inflight, next_action, waiting;
    unsigned int max_sync_attempts, sync_attempts;
} cdjr_link;

int cdjr_init(cdjr_link *link, uint32_t timeout_ticks);
/* Optional application-readiness check after STATUS. Zero attempts disables
 * it for legacy users. Configure only once, while idle. */
int cdjr_configure(cdjr_link *link, uint32_t settle_ticks,
                   unsigned int max_sync_attempts);
int cdjr_begin(cdjr_link *link, uint32_t now);
/* 1: available, 0: none, -1: terminal. Nonzero tokens never repeat. */
int cdjr_poll(cdjr_link *link, uint32_t now, cdjr_action *action,
              uint32_t *token);
/* Complete an accepted command. peer_ready is used only for READ_READY.
 * 1: advanced, 0: ignored/drained, -1: failed. Call from the owner loop,
 * using a token captured when submitting, not a mutable current token. */
int cdjr_complete(cdjr_link *link, uint32_t token, uint32_t result,
                   int peer_ready, uint32_t now);
/* Legacy no-delay completion; new adapters should use complete(). */
int cdjr_callback(cdjr_link *link, uint32_t token, uint32_t result);
/* Submission returned false: no callback/buffer ownership was acquired. */
int cdjr_rejected(cdjr_link *link, uint32_t token);
/* Stop scheduling; an already accepted command must drain before begin().
 * This never cancels SI, clears a timeout, or changes command buffers. */
int cdjr_interrupt(cdjr_link *link);
#endif
