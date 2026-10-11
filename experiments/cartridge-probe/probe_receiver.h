#ifndef CUBEDECK_PROBE_RECEIVER_H
#define CUBEDECK_PROBE_RECEIVER_H
#include "joyboot_si_adapter.h"
#include <stdint.h>
typedef struct {
    cd_gcn_si si;
    uint32_t deadline, token, raw;
    int state, valid;
} cd_probe_receiver;
/* Return 1 for decoded result, 0 while waiting, -1 on error.
 * This is a second SI read after the upload's final BIOS response,
 * not a validation of that BIOS response. */
int cd_probe_receiver_step(cd_probe_receiver *r, unsigned port,
                           uint32_t frame, uint32_t timeout);
#endif
