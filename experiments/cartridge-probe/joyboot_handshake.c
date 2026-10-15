#include "joyboot_handshake.h"
#include "cubedeck/gba_joyboot_words.h"
#include <string.h>
int cd_jb_handshake_init(cd_jb_handshake *h, unsigned port, size_t image_size)
{
    cd_jb_handshake clean;
    if (!h || port > 3u) return -1;
    memset(&clean, 0, sizeof(clean));
    if (cd_jb_client_key(image_size, &clean.client_key) != 0) return -1;
    clean.port = port;
    memcpy(h, &clean, sizeof(clean));
    return 0;
}
int cd_jb_handshake_step(cd_jb_handshake *h, uint32_t frame, uint32_t timeout)
{
    uint32_t token, result;
    u32 reply;
    u8 cmd[5] = {0u, 0u, 0u, 0u, 0u};
    u32 tx = 1u, rx = 3u;
    if (!h || !timeout || timeout >= 0x80000000u || h->errors) return -1;
    if (h->awaiting) {
        if (cd_gcn_si_take(&h->si, &token, &result, &reply) == 0) {
            h->awaiting = 0u;
            if (token != h->token || result ||
                (int32_t)(frame - h->deadline) >= 0) {
                h->errors = 1u;
                return -1;
            }
            if (h->step == 0u) h->step = 1u;
            else if (h->step == 1u) {
                h->last_status = reply;
                /* GBA BIOS boot-ready flag in the 3-byte STATUS reply. */
                if ((h->si.response[2] & 0x10u) == 0u) {
                    h->errors = 2u; return -1;
                }
                h->step = 2u;
            } else if (h->step == 2u) {
                h->raw_session_key = reply;
                h->step = 3u;
            } else if (h->step == 3u) h->step = 4u;
            else { h->errors = 3u; return -1; }
        } else if ((int32_t)(frame - h->deadline) >= 0) {
            /* SI buffers remain owned even after terminal timeout. */
            h->errors = 4u;
            return -1;
        }
        if (h->awaiting) return 0;
    }
    if (h->step == 4u) return 1;
    if (h->step == 0u) cmd[0] = 0xffu;
    else if (h->step == 1u) cmd[0] = 0x00u;
    else if (h->step == 2u) { cmd[0] = 0x14u; rx = 5u; }
    else if (h->step == 3u) {
        u32 swapped = ((h->client_key & 0xffu) << 24) |
            ((h->client_key & 0xff00u) << 8) |
            ((h->client_key >> 8) & 0xff00u) |
            (h->client_key >> 24);
        unsigned i;
        cmd[0] = 0x15u; tx = 5u; rx = 1u;
        for (i = 0u; i < 4u; ++i) cmd[i + 1u] = (u8)(swapped >> (8u * i));
    } else return -1;
    ++h->token;
    if (cd_gcn_si_submit(&h->si, h->port, cmd, tx, rx,
                         h->token, frame) != 0) {
        h->errors = 5u; return -1;
    }
    h->deadline = frame + timeout;
    h->awaiting = 1u;
    return 0;
}
