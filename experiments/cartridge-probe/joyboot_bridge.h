#ifndef CUBEDECK_JOYBOOT_BRIDGE_H
#define CUBEDECK_JOYBOOT_BRIDGE_H
#include "joyboot_si_adapter.h"
#include "cubedeck/joyboot_sender.h"
typedef struct {
    cd_gcn_si si;
    cd_jb_sender sender;
    unsigned int port;
    uint32_t submitted_token;
    int awaiting;
    int awaiting_final_read;
    uint32_t final_reply_word;
    int final_reply_available;
} cd_jb_bridge;
/* Stage begins after successful BIOS recognition and session-key exchange.
 * Bridge transfers only the encoded image and CRC, not BIOS handshake. */
int cd_jb_bridge_init(cd_jb_bridge *b, unsigned int port,
                      const uint8_t *image, size_t size, uint32_t session_key);
/* Return 1 only when final SI read has completed; it is NOT proof of
 * BIOS acceptance. Caller must validate final_reply_word separately. */
int cd_jb_bridge_step(cd_jb_bridge *b, uint32_t frame, uint32_t timeout);
#endif
