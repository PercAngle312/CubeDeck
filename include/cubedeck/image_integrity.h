#ifndef CUBEDECK_IMAGE_INTEGRITY_H
#define CUBEDECK_IMAGE_INTEGRITY_H

#include <stddef.h>
#include <stdint.h>

/* Portable RAM-only whole-transfer integrity check.
 * No storage, memory-card, or cartridge access.
 */
typedef struct {
    size_t expected_bytes;
    uint32_t expected_crc32;
} cdim_manifest;

uint32_t cdim_crc32(const uint8_t *bytes, size_t length);

/* Returns 1 for complete and matching, 0 for incomplete, -1 for invalid
 * arguments, overlong input, or mismatching finished image.
 */
int cdim_verify(const cdim_manifest *manifest,
                const uint8_t *bytes, size_t available_bytes);

#endif
