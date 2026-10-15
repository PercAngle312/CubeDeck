#ifndef CUBEDECK_JOYBOOT_READY_H
#define CUBEDECK_JOYBOOT_READY_H
#include "joyboot_si_adapter.h"
/* Isolated preflight. Sends only RESET, STATUS and READ; never WRITE (0x15).
 * Zero-initialize once. Keep this object alive after failure: SI has no cancel.
 * A new attempt requires restarting the DOL; start cannot reset an active bus. */
enum cd_jbr_phase { CD_JBR_IDLE, CD_JBR_RESET, CD_JBR_STATUS,
                    CD_JBR_KEY, CD_JBR_READY, CD_JBR_FAILED };
enum cd_jbr_error { CD_JBR_OK, CD_JBR_BAD_ARGUMENT, CD_JBR_TIMEOUT,
                    CD_JBR_CALLBACK_ERROR, CD_JBR_TOKEN_ERROR,
                    CD_JBR_NOT_READY, CD_JBR_BUSY };
typedef struct {
    cd_gcn_si si;
    unsigned port, phase, error, status_polls, refused;
    uint32_t token, deadline, expires, retry_at, last_reply, last_result;
    uint32_t session_key;
    unsigned awaiting, failed_phase;
} cd_jbr;
int cd_jbr_start(cd_jbr *r, unsigned port, uint32_t now, uint32_t timeout);
/* 1: boot-ready STATUS and completed session-key read; not uploaded/executed.
 * 0: progress, -1: terminal failure; never cancels pending hardware activity. */
int cd_jbr_step(cd_jbr *r, uint32_t now, uint32_t timeout);
#endif
