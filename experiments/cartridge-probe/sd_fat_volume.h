#ifndef CD_SD_FAT_VOLUME_H
#define CD_SD_FAT_VOLUME_H
#include <stdint.h>
/* 1 = supported FAT BPB, 0 = unsupported or malformed. No label dependency. */
int cd_sd_fat_bpb(const uint8_t sector[512], uint32_t extent);
/* Valid nonempty primary FAT partition, bounded against 32-bit LBA overflow. */
int cd_sd_fat_partition(const uint8_t sector[512], unsigned index,
                        uint32_t *lba, uint32_t *extent);
#endif
