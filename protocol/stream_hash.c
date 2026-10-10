#include "cubedeck/stream_hash.h"

int cdsh_source_crc32(cdtx_source reader, void *context,
                      size_t length, uint32_t *out_crc32)
{
    uint8_t chunk[CDTP_MAX_PAYLOAD];
    size_t offset = 0u;
    uint32_t state = 0xFFFFFFFFu;
    if (!reader || !out_crc32 || length > CDST_MAX_BYTES)
        return -1;
    while (offset < length) {
        size_t count = length - offset;
        size_t i;
        if (count > sizeof(chunk))
            count = sizeof(chunk);
        if (reader(context, offset, chunk, count) != 0)
            return -1;
        for (i = 0u; i < count; ++i) {
            unsigned int bit;
            state ^= chunk[i];
            for (bit = 0u; bit < 8u; ++bit)
                state = (state >> 1) ^ ((state & 1u) ? 0xEDB88320u : 0u);
        }
        offset += count;
    }
    *out_crc32 = state ^ 0xFFFFFFFFu;
    return 0;
}
