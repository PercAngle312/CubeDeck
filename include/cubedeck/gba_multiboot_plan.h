#ifndef CUBEDECK_GBA_MULTIBOOT_PLAN_H
#define CUBEDECK_GBA_MULTIBOOT_PLAN_H
#include <stddef.h>
#include <stdint.h>
/* This module validates candidate images; it does not communicate with a GBA. */
#define CD_MB_HEADER_BYTES 0xC0u
#define CD_MB_MIN_IMAGE_BYTES 0x200u
#define CD_MB_MAX_IMAGE_BYTES 0x3FF40u
typedef struct {
    uint32_t image_bytes;
    uint32_t payload_bytes;
    uint32_t halfwords_in_header;
} cd_mb_plan;
int cd_mb_plan_image(const uint8_t *data, size_t length, cd_mb_plan *out);
#endif
