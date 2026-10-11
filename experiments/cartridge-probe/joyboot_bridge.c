#include "joyboot_bridge.h"
#include <string.h>
int cd_jb_bridge_init(cd_jb_bridge *b, unsigned int port,
                      const uint8_t *image, size_t size, uint32_t key)
{
    cd_jb_bridge clean;
    if (!b || port > 3u || !image) return -1;
    memset(&clean, 0, sizeof(clean));
    if (cd_jb_sender_init(&clean.sender, image, size, key) != 0) return -1;
    clean.port = port;
    memcpy(b, &clean, sizeof(clean));
    return 0;
}
int cd_jb_bridge_step(cd_jb_bridge *b, uint32_t frame, uint32_t timeout)
{
    uint32_t token, result;
    u32 word;
    uint8_t command[5];
    if (!b) return -1;
    /* Always drain SI mailbox first. A timeout must not free ownership;
     * terminal states require a fresh process/adapter, not a retry. */
    if (b->awaiting && cd_gcn_si_take(&b->si, &token, &result, &word) == 0) {
        b->awaiting = 0;
        if (token != b->submitted_token) return -1;
        if (b->awaiting_final_read) {
            b->awaiting_final_read = 0;
            if (cd_jb_sender_complete_final_read(&b->sender, token,
                                                  result, frame) != 0)
                return -1;
            b->final_reply_word = word; /* Untrusted until caller checks it. */
            b->final_reply_available = 1;
        } else if (cd_jb_sender_complete(&b->sender, token, result, frame) != 0)
            return -1;
    }
    if (cd_jb_sender_poll(&b->sender, frame) != 0) return -1;
    if (b->awaiting) return 0;
    if (b->sender.phase == 3) {
        const uint8_t read_cmd = 0x14u;
        if (b->final_reply_available) return 1; /* Not BIOS acceptance. */
        if (cd_jb_sender_prepare_final_read(&b->sender, frame, timeout,
                                             &token) != 0) return -1;
        if (cd_gcn_si_submit(&b->si, b->port, &read_cmd, 1u, 5u,
                             token, frame) != 0) {
            /* Unlike data words, a refused final read is terminal until
             * a new session; never let its queue remain falsely owned. */
            b->sender.phase = -1;
            return -1;
        }
        b->submitted_token = token;
        b->awaiting_final_read = 1;
        b->awaiting = 1;
        return 0;
    }
    if (b->sender.phase != 1 && b->sender.phase != 2) return -1;
    if (cd_jb_sender_prepare(&b->sender, frame, timeout, command, &token) != 0)
        return -1;
    if (cd_gcn_si_submit(&b->si, b->port, command, 5u, 1u, token, frame) != 0) {
        if (cd_jb_sender_reject_unsent(&b->sender, token) != 0) return -1;
        return 0; /* Submission refused: exact same word is retryable. */
    }
    b->submitted_token = token;
    b->awaiting = 1;
    return 0;
}
