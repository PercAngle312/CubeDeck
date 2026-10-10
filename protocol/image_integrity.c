#include "cubedeck/image_integrity.h"
#include "cubedeck/chunk_session.h"

uint32_t cdim_crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    if (length != 0u && bytes == NULL)
        return 0u;
    for (i = 0; i < length; ++i) {
        unsigned int bit;
        crc ^= bytes[i];
        for (bit = 0; bit < 8u; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

int cdim_verify(const cdim_manifest *manifest,
                const uint8_t *bytes, size_t available_bytes)
{
    if (manifest == NULL ||
        manifest->expected_bytes > CDCS_MAX_IMAGE ||
        available_bytes > CDCS_MAX_IMAGE ||
        (available_bytes != 0u && bytes == NULL))
        return -1;
    if (available_bytes < manifest->expected_bytes)
        return 0;
    if (available_bytes != manifest->expected_bytes)
        return -1;
    return cdim_crc32(bytes, available_bytes) == manifest->expected_crc32 ? 1 : -1;
}
