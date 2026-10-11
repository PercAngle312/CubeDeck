#include "sd_fat_volume.h"
static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p)
{ return le16(p) | le16(p + 2) << 16; }
int cd_sd_fat_bpb(const uint8_t s[512], uint32_t extent)
{
    uint32_t reserved, fats, fat_size, total, roots, overhead, clusters;
    if (s[510] != 0x55 || s[511] != 0xaa || le16(s + 11) != 512 ||
        !s[13] || (s[13] & (s[13] - 1)) || s[13] > 128) return 0;
    reserved = le16(s + 14); fats = s[16]; roots = le16(s + 17);
    fat_size = le16(s + 22); total = le16(s + 19);
    if (!fat_size) fat_size = le32(s + 36);
    if (!total) total = le32(s + 32);
    if (!reserved || !fats || fats > 2 || !fat_size || !total ||
        (extent && total > extent)) return 0;
    if ((uint64_t)reserved + (uint64_t)fats * fat_size +
        ((uint64_t)roots * 32 + 511) / 512 >= total) return 0;
    overhead = reserved + fats * fat_size + (roots * 32 + 511) / 512;
    clusters = (total - overhead) / s[13];
    if (!clusters) return 0;
    if (clusters >= 65525) {
        uint32_t root = le32(s + 44);
        if (roots || le16(s + 22) || le16(s + 42) ||
            root < 2 || root >= clusters + 2 || clusters > 0x0ffffff5u ||
            (uint64_t)fat_size * 512 / 4 < (uint64_t)clusters + 2) return 0;
    } else if (!roots || !le16(s + 22)) return 0;
    return 1;
}
int cd_sd_fat_partition(const uint8_t s[512], unsigned i,
                        uint32_t *lba, uint32_t *extent)
{
    const uint8_t *p;
    uint32_t start, count;
    if (i >= 4 || s[510] != 0x55 || s[511] != 0xaa) return 0;
    p = s + 446 + i * 16;
    if (p[0] != 0 && p[0] != 0x80) return 0;
    if (p[4] != 1 && p[4] != 4 && p[4] != 6 && p[4] != 0x0b &&
        p[4] != 0x0c && p[4] != 0x0e && p[4] != 0x11 &&
        p[4] != 0x14 && p[4] != 0x16 && p[4] != 0x1b &&
        p[4] != 0x1c && p[4] != 0x1e) return 0;
    start = le32(p + 8); count = le32(p + 12);
    if (!start || !count || (uint64_t)start + count > UINT32_MAX) return 0;
    *lba = start; *extent = count;
    return 1;
}
