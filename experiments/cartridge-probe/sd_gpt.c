#include "sd_gpt.h"
#include "cubedeck/image_integrity.h"
#include <string.h>
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t le64(const uint8_t *p)
{ return le32(p) | (uint64_t)le32(p + 4) << 32; }
static int empty_guid(const uint8_t *p)
{
    unsigned i;
    for (i = 0; i < 16; ++i) if (p[i]) return 0;
    return 1;
}
int cd_sd_gpt_read(cd_sd_sector_reader read, void *ctx,
                   cd_sd_gpt_work *w, cd_sd_gpt_volumes *out)
{
    /* Microsoft Basic Data, in GPT's mixed-endian on-disk GUID encoding. */
    static const uint8_t basic[16] = {
        0xa2,0xa0,0xd0,0xeb,0xe5,0xb9,0x33,0x44,
        0x87,0xc0,0x68,0xb6,0xb7,0x26,0x99,0xc7
    };
    uint32_t size, crc, entries, bytes, sectors, table_crc;
    uint64_t first, last, alternate, table_lba;
    unsigned i, j, count = 0;
    if (!out) return -2;
    memset(out, 0, sizeof(*out));
    if (!read || !w) return -2;
    if (!read(ctx, 1, w->header)) return -1;
    size = le32(w->header + 12);
    if (memcmp(w->header, "EFI PART", 8) ||
        le32(w->header + 8) != 0x00010000u || size < 92 || size > 512 ||
        le32(w->header + 20) || le64(w->header + 24) != 1 ||
        empty_guid(w->header + 56)) return -2;
    crc = le32(w->header + 16);
    memset(w->header + 16, 0, 4);
    if (cdim_crc32(w->header, size) != crc) return -2;
    first = le64(w->header + 40); last = le64(w->header + 48);
    alternate = le64(w->header + 32); table_lba = le64(w->header + 72);
    entries = le32(w->header + 80);
    if (!entries || entries > CD_SD_GPT_MAX_ENTRIES ||
        le32(w->header + 84) != CD_SD_GPT_ENTRY_BYTES) return -3;
    bytes = entries * CD_SD_GPT_ENTRY_BYTES;
    sectors = (bytes + 511) / 512;
    if (first < 34 || last < first || alternate <= last ||
        alternate > UINT32_MAX || table_lba < 2 || table_lba > UINT32_MAX ||
        table_lba + sectors > first || last + 1 >= alternate ||
        /* Reserve the backup table immediately before its header. */
        alternate - last <= sectors) return -3;
    for (i = 0; i < sectors; ++i)
        if (!read(ctx, (uint32_t)table_lba + i, w->table + i * 512)) return -1;
    table_crc = le32(w->header + 88);
    if (cdim_crc32(w->table, bytes) != table_crc) return -4;
    /* Validate every used entry, including partitions we will not mount. */
    for (i = 0; i < entries; ++i) {
        const uint8_t *p = w->table + i * CD_SD_GPT_ENTRY_BYTES;
        uint64_t a, b;
        if (empty_guid(p)) continue;
        a = le64(p + 32); b = le64(p + 40);
        if (empty_guid(p + 16) || a < first || b > last || b < a ||
            b - a + 1 > UINT32_MAX) return -5;
        for (j = 0; j < i; ++j) {
            const uint8_t *q = w->table + j * CD_SD_GPT_ENTRY_BYTES;
            if (empty_guid(q)) continue;
            if (!memcmp(p + 16, q + 16, 16) ||
                (a <= le64(q + 40) && le64(q + 32) <= b)) return -5;
        }
    }
    /* Publish only after the whole table is authenticated and validated. */
    out->entries = entries; out->table_lba = (uint32_t)table_lba;
    for (i = 0; i < entries; ++i) {
        const uint8_t *p = w->table + i * CD_SD_GPT_ENTRY_BYTES;
        if (memcmp(p, basic, 16)) continue;
        out->lba[count] = (uint32_t)le64(p + 32);
        out->extent[count] = (uint32_t)(le64(p + 40) - le64(p + 32) + 1);
        out->index[count++] = i + 1;
    }
    out->count = count;
    return 0;
}
