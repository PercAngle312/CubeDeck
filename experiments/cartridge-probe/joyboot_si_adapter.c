#include "joyboot_si_adapter.h"
#include <ogc/irq.h>
#include <string.h>
/* libogc callback has no user context. One isolated owner at a time. */
static cd_gcn_si *owner;
static unsigned int owner_port;
static void on_transfer(s32 channel, u32 result)
{
    cd_gcn_si *bus = owner;
    if (!bus || (unsigned int)channel != owner_port ||
        !bus->active || bus->done) return;
    bus->result = result;
    bus->done = 1u;
}
int cd_gcn_si_submit(cd_gcn_si *bus, unsigned int port,
                     const u8 *cmd, u32 txlen, u32 rxlen,
                     uint32_t token, uint32_t frame)
{
    u32 irq, accepted;
    if (!bus || !cmd || !txlen || txlen > 32u || rxlen > 32u ||
        port > 3u) return -1;
    irq = IRQ_Disable();
    if (owner || bus->active) { IRQ_Restore(irq); return -1; }
    memset(bus->command, 0, sizeof(bus->command));
    memset(bus->response, 0, sizeof(bus->response));
    memcpy(bus->command, cmd, txlen);
    bus->token = token;
    bus->submitted_frame = frame;
    bus->result = bus->done = 0u;
    bus->active = 1u;
    owner = bus;
    owner_port = port;
    accepted = SI_Transfer((s32)port, bus->command, txlen,
                           bus->response, rxlen, on_transfer, 50);
    if (!accepted) {
        owner = NULL;
        bus->active = 0u;
    }
    IRQ_Restore(irq);
    return accepted ? 0 : -1;
}
int cd_gcn_si_take(cd_gcn_si *bus, uint32_t *token, uint32_t *result,
                   u32 *word)
{
    u32 irq;
    if (!bus || !token || !result || !word) return -1;
    irq = IRQ_Disable();
    if (owner != bus || !bus->active || !bus->done) {
        IRQ_Restore(irq);
        return -1;
    }
    *token = bus->token;
    *result = bus->result;
    *word = (u32)bus->response[0] | ((u32)bus->response[1] << 8) |
            ((u32)bus->response[2] << 16) |
            ((u32)bus->response[3] << 24);
    bus->done = bus->active = 0u;
    owner = NULL;
    IRQ_Restore(irq);
    return 0;
}
