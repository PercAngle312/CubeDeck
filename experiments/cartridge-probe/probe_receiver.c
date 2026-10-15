#include "probe_receiver.h"
#include "cubedeck/cart_probe_reply.h"
#include <stdint.h>
#define CD_PROBE_MAX_ATTEMPTS 5u
#define CD_PROBE_RETRY_DELAY 30u
int cd_probe_receiver_step(cd_probe_receiver *r, unsigned port,
                           uint32_t frame, uint32_t timeout)
{
    const u8 cmd = 0x14u;
    uint32_t token, result;
    u32 reply;
    if (!r || port > 3u || !timeout || timeout >= 0x80000000u)
        return -1;
    if (r->state == 3) return 1;
    if (r->state < 0) return -1;
    if (r->state == 2) {
        if ((int32_t)(frame - r->retry_at) < 0) return 0;
        r->state = 0;
    }
    if (r->state == 0) {
        if (r->attempts >= CD_PROBE_MAX_ATTEMPTS) {
            r->state = -1;
            return -1;
        }
        if (cd_gcn_si_submit(&r->si, port, &cmd, 1u, 5u,
                             (uint32_t)r->attempts + 1u, frame) != 0) {
            r->state = -1;
            return -1;
        }
        ++r->attempts;
        r->deadline = frame + timeout;
        r->state = 1;
        return 0;
    }
    if (cd_gcn_si_take(&r->si, &token, &result, &reply) == 0) {
        if (token != (uint32_t)r->attempts || result ||
            (int32_t)(frame - r->deadline) >= 0) {
            r->state = -1;
            return -1;
        }
        if (cd_cart_probe_decode(reply, &r->valid) != 0) {
            if (r->attempts >= CD_PROBE_MAX_ATTEMPTS) {
                r->state = -1;
                return -1;
            }
            r->retry_at = frame + CD_PROBE_RETRY_DELAY;
            r->state = 2;
            return 0;
        }
        r->raw = reply;
        r->state = 3;
        return 1;
    }
    if ((int32_t)(frame - r->deadline) >= 0) {
        /* Native SI buffers must not be reused while callback outstanding. */
        r->state = -1;
        return -1;
    }
    return 0;
}
