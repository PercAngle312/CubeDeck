#ifndef CD_SD_GPT_H
#define CD_SD_GPT_H
#include <stdint.h>
#define CD_SD_GPT_MAX_ENTRIES 128u
#define CD_SD_GPT_ENTRY_BYTES 128u
typedef int (*cd_sd_sector_reader)(void *, uint32_t, void *);
typedef struct {
    _Alignas(32) uint8_t header[512];
    _Alignas(32) uint8_t table[CD_SD_GPT_MAX_ENTRIES * CD_SD_GPT_ENTRY_BYTES];
} cd_sd_gpt_work;
typedef struct {
    unsigned count, entries;
    uint32_t table_lba;
    uint32_t lba[CD_SD_GPT_MAX_ENTRIES], extent[CD_SD_GPT_MAX_ENTRIES];
    unsigned index[CD_SD_GPT_MAX_ENTRIES];
} cd_sd_gpt_volumes;
/* Primary GPT only, 512-byte sectors, <=128 128-byte entries, 32-bit LBAs.
 * Output is empty on every failure; no volume reads until all CRC/range checks.
 * Errors: -1 I/O, -2 header format/CRC, -3 unsupported bounds,
 * -4 table CRC, -5 malformed/overlapping partitions. Never writes storage. */
int cd_sd_gpt_read(cd_sd_sector_reader read, void *ctx,
                   cd_sd_gpt_work *work, cd_sd_gpt_volumes *out);
#endif
