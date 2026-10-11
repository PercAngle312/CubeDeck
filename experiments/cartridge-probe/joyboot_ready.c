#include "joyboot_ready.h"
#define STATUS_DELAY 15u
#define MAX_STATUS_POLLS 20u
static int fail(cd_jbr *r, unsigned error)
{
    r->failed_phase = r->phase;
    r->error = error;
    r->phase = CD_JBR_FAILED;
    return -1;
}
static int expired(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}
int cd_jbr_start(cd_jbr *r, unsigned port, uint32_t now, uint32_t timeout)
{
    /* Never clear or replace a previous adapter, including timed-out buffers. */
    if (!r || port > 3u || !timeout || timeout >= 0x10000000u ||
        r->phase != CD_JBR_IDLE || r->si.active) return -1;
    r->port = port;
    r->phase = CD_JBR_RESET;
    r->expires = now + timeout * 4u;
    r->deadline = now + timeout;
    r->retry_at = now;
    return 0;
}
int cd_jbr_step(cd_jbr *r, uint32_t now, uint32_t timeout)
{
    uint32_t token, result;
    u32 reply;
    u8 cmd;
    if (!r) return -1;
    if (r->phase == CD_JBR_READY) return 1;
    if (r->phase == CD_JBR_FAILED) return -1;
    if (r->phase == CD_JBR_IDLE || !timeout || timeout >= 0x10000000u)
        return fail(r, CD_JBR_BAD_ARGUMENT);
    if (r->awaiting && cd_gcn_si_take(&r->si, &token, &result, &reply) == 0) {
        r->awaiting = 0;
        r->last_reply = reply;
        r->last_result = result;
        if (token != r->token) return fail(r, CD_JBR_TOKEN_ERROR);
        if (result) return fail(r, CD_JBR_CALLBACK_ERROR);
        if (expired(now, r->deadline) || expired(now, r->expires))
            return fail(r, CD_JBR_TIMEOUT);
        if (r->phase == CD_JBR_RESET) r->phase = CD_JBR_STATUS;
        else if (r->phase == CD_JBR_STATUS) {
            if (r->si.response[2] & 0x10u) r->phase = CD_JBR_KEY;
            else {
                if (r->status_polls >= MAX_STATUS_POLLS)
                    return fail(r, CD_JBR_NOT_READY);
                r->retry_at = now + STATUS_DELAY;
            }
        } else if (r->phase == CD_JBR_KEY) {
            r->session_key = reply;
            r->phase = CD_JBR_READY;
            return 1;
        } else return fail(r, CD_JBR_BAD_ARGUMENT);
        r->deadline = now + timeout;
    }
    if (expired(now, r->expires) || expired(now, r->deadline))
        return fail(r, r->awaiting ? CD_JBR_TIMEOUT : CD_JBR_BUSY);
    if (r->awaiting || !expired(now, r->retry_at)) return 0;
    cmd = r->phase == CD_JBR_RESET ? 0xffu :
          r->phase == CD_JBR_STATUS ? 0x00u : 0x14u;
    if (cd_gcn_si_submit(&r->si, r->port, &cmd, 1u,
                         r->phase == CD_JBR_KEY ? 5u : 3u,
                         r->token + 1u, now) != 0) {
        ++r->refused;
        /* Refused submission owns no new buffers. Retry within the existing
         * deadline; do not extend it on each busy response. */
        return 0;
    }
    ++r->token;
    if (r->phase == CD_JBR_STATUS) ++r->status_polls;
    r->awaiting = 1u;
    r->deadline = now + timeout;
    return 0;
}
