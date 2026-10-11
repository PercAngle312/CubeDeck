#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "probe_receiver.h"
#include "cubedeck/cart_probe_reply.h"
static unsigned submit_count;
static int deliver;
static uint32_t reply_word;
int cd_gcn_si_submit(cd_gcn_si *bus, unsigned port, const u8 *cmd,
                     u32 txlen, u32 rxlen, uint32_t token, uint32_t frame)
{
    assert(port == 1u && cmd[0] == 0x14u && txlen == 1u && rxlen == 5u);
    assert(!bus->active);
    bus->active = 1u;
    bus->token = token;
    bus->submitted_frame = frame;
    ++submit_count;
    return 0;
}
int cd_gcn_si_take(cd_gcn_si *bus, uint32_t *token, uint32_t *result, u32 *word)
{
    if (!deliver || !bus->active) return -1;
    bus->active = 0u;
    *token = bus->token;
    *result = 0u;
    *word = reply_word;
    return 0;
}
int main(void)
{
    cd_probe_receiver r = {0};
    unsigned i;
    assert(cd_probe_receiver_step(&r, 1u, 100u, 10u) == 0);
    assert(submit_count == 1u && r.state == 1);
    reply_word = 0x12345678u;
    deliver = 1;
    assert(cd_probe_receiver_step(&r, 1u, 101u, 10u) == 0);
    assert(r.state == 2 && r.attempts == 1u);
    assert(cd_probe_receiver_step(&r, 1u, 130u, 10u) == 0);
    assert(submit_count == 1u);
    assert(cd_probe_receiver_step(&r, 1u, 131u, 10u) == 0);
    assert(submit_count == 2u);
    reply_word = cd_cart_probe_reply(1);
    assert(cd_probe_receiver_step(&r, 1u, 132u, 10u) == 1);
    assert(r.valid == 1 && r.raw == reply_word && r.state == 3);
    assert(cd_probe_receiver_step(&r, 1u, 133u, 10u) == 1);
    memset(&r, 0, sizeof(r));
    reply_word = 0u;
    submit_count = 0u;
    for (i = 0; i < 5u; ++i) {
        uint32_t frame = 200u + 31u * i;
        assert(cd_probe_receiver_step(&r, 1u, frame, 10u) == 0);
        assert(cd_probe_receiver_step(&r, 1u, frame + 1u, 10u) == (i == 4u ? -1 : 0));
    }
    assert(submit_count == 5u && r.state == -1);
    memset(&r, 0, sizeof(r));
    deliver = 0;
    assert(cd_probe_receiver_step(&r, 1u, 500u, 10u) == 0);
    assert(cd_probe_receiver_step(&r, 1u, 510u, 10u) == -1);
    assert(r.state == -1 && r.si.active == 1u);
    puts("Cartridge status receiver: delayed readiness, retry cap and SI timeout passed.");
    return 0;
}
