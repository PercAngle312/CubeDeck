#ifndef CUBEDECK_RAM_V10_LINK_H
#define CUBEDECK_RAM_V10_LINK_H
/* Separate v10 runtime link; do not use until multiboot final-read has
 * finished and the GBA application has started. Static storage only.
 * No cartridge access or filesystem writes. */
#include "joyboot_si_adapter.h"
#include "ram_v10_client.h"
typedef struct {
    cd_gcn_si si;
    cd_v10_client client;
    uint32_t token, deadline, retry_at, last_result;
    unsigned awaiting, active_phase, reads, refusals, error;
} cd_v10_link;
/* Caller initializes zeroed static link after successful RAM upload.
 * On errors, retain it until restart; no cancellation of active SI DMA. */
int cd_v10_link_start(cd_v10_link *link, uint16_t sequence, uint32_t frame);
int cd_v10_link_step(cd_v10_link *link, uint32_t frame);
#endif
