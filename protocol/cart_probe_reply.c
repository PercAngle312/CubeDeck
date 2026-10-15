#include "cubedeck/cart_probe_reply.h"
uint32_t cd_cart_probe_reply(int valid)
{
    return CD_CART_PROBE_MAGIC |
           (valid ? CD_CART_PROBE_VALID : CD_CART_PROBE_FAILED) |
           CD_CART_PROBE_VERSION;
}
int cd_cart_probe_decode(uint32_t word, int *valid)
{
    uint32_t status = word & 0x0000ff00u;
    if (!valid || (word & 0xffff00ffu) !=
        (CD_CART_PROBE_MAGIC | CD_CART_PROBE_VERSION)) return -1;
    if (status != CD_CART_PROBE_VALID && status != CD_CART_PROBE_FAILED)
        return -1;
    *valid = status == CD_CART_PROBE_VALID;
    return 0;
}
