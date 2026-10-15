#include "cubedeck/gba_cart_probe.h"
#include <string.h>
int cd_gba_probe_header(cd_gba_read_byte reader, void *ctx,
                        cd_gba_cart_probe *probe)
{
    size_t i;
    cd_gba_cart_probe local;
    if (!reader || !probe) return -1;
    memset(&local, 0, sizeof(local));
    /* Fixed 192-byte ROM header only. No saves, writes, bus probing or
     * guesses about ROM length. Caller controls actual hardware access. */
    for (i = 0; i < CD_GBA_CART_HEADER_LIMIT; ++i) {
        if (reader(ctx, (uint32_t)i, &local.bytes[i]) != 0) return -1;
    }
    if (cd_gba_parse_header(local.bytes, sizeof(local.bytes), &local.parsed) != 0 ||
        !local.parsed.checksum_valid) return -1;
    memcpy(probe, &local, sizeof(local));
    return 0;
}
