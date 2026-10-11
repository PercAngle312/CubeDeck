#include "cubedeck/gba_multiboot_plan.h"
#include "cubedeck/gba_cart_header.h"
#include <string.h>
int cd_mb_plan_image(const uint8_t *data, size_t length, cd_mb_plan *out)
{
    cd_gba_cart_header header;
    cd_mb_plan planned;
    if (!data || !out || length < CD_MB_MIN_IMAGE_BYTES ||
        length > CD_MB_MAX_IMAGE_BYTES || (length & 15u)) return -1;
    if (cd_gba_parse_header(data, length, &header) != 0 ||
        !header.checksum_valid || data[0xB2u] != 0x96u) return -1;
    /* Do not guess payload provenance or assume valid checksum means the
     * Nintendo-logo or entrypoint bytes have been authenticated. */
    planned.image_bytes = (uint32_t)length;
    planned.payload_bytes = (uint32_t)length - CD_MB_HEADER_BYTES;
    planned.halfwords_in_header = CD_MB_HEADER_BYTES / 2u;
    memcpy(out, &planned, sizeof(planned));
    return 0;
}
