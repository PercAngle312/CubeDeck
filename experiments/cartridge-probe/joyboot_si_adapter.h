#ifndef CUBEDECK_JOYBOOT_SI_ADAPTER_H
#define CUBEDECK_JOYBOOT_SI_ADAPTER_H
#include <gccore.h>
#include <stdint.h>
/* One outstanding SI transfer; must remain alive until callback is drained.
 * Timeout deliberately does not release DMA ownership. */
typedef struct {
    u8 command[32] ATTRIBUTE_ALIGN(32);
    u8 response[32] ATTRIBUTE_ALIGN(32);
    volatile uint32_t active, done, result, token;
    uint32_t submitted_frame;
} cd_gcn_si;
int cd_gcn_si_submit(cd_gcn_si *bus, unsigned int port,
                     const u8 *cmd, u32 txlen, u32 rxlen,
                     uint32_t token, uint32_t frame);
int cd_gcn_si_take(cd_gcn_si *bus, uint32_t *token, uint32_t *result,
                   u32 *word);
#endif
