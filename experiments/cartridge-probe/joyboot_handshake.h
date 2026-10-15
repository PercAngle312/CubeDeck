#ifndef CUBEDECK_JOYBOOT_HANDSHAKE_H
#define CUBEDECK_JOYBOOT_HANDSHAKE_H
#include "joyboot_si_adapter.h"
#include <stdint.h>
typedef struct {
    cd_gcn_si si;
    unsigned port, step, awaiting;
    uint32_t token, deadline, raw_session_key, client_key;
    uint32_t last_status, errors;
} cd_jb_handshake;
/* RESET -> STATUS (ready bit) -> READ session key -> SEND client key.
 * step 4 is ready to initialize the image sender. One attempt only.
 * This is an isolated experimental GameCube adapter. */
int cd_jb_handshake_init(cd_jb_handshake *h, unsigned port, size_t image_size);
int cd_jb_handshake_step(cd_jb_handshake *h, uint32_t frame, uint32_t timeout);
#endif
