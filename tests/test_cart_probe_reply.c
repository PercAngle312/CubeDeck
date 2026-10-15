#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "cubedeck/cart_probe_reply.h"
int main(void)
{
    int valid = -1;
    uint32_t good = cd_cart_probe_reply(1);
    uint32_t bad = cd_cart_probe_reply(0);
    assert(cd_cart_probe_decode(good, &valid) == 0 && valid == 1);
    assert(cd_cart_probe_decode(bad, &valid) == 0 && valid == 0);
    assert(cd_cart_probe_decode(good ^ 0x10000u, &valid) == -1);
    assert(cd_cart_probe_decode(good | 0x200u, &valid) == -1);
    assert(cd_cart_probe_decode(good ^ 1u, &valid) == -1);
    assert(cd_cart_probe_decode(good, 0) == -1);
    puts("Read-only cartridge status protocol: valid, invalid and malformed replies checked.");
    return 0;
}
