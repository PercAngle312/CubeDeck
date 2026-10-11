#include <gccore.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "sd_mount.h"
#include "sd_fat_volume.h"
static uint8_t mbr[512], vbr[512];
static int start_ok = 1, inserted = 1, fail0, failfirst, mount_ok = 1;
static unsigned mounts;
static uint32_t mounted;
static bool startup(void) { return start_ok; }
static bool present(void) { return inserted; }
const DISC_INTERFACE __io_gcsd2 = {0, 3, startup, present, 0, 0, 0, 0};
static void put32(uint8_t *p, uint32_t n)
{ for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(n >> (8 * i)); }
static void fat32(void)
{
    memset(vbr, 0, 512);
    vbr[510] = 0x55; vbr[511] = 0xaa;
    vbr[11] = 0; vbr[12] = 2; vbr[13] = 8;
    vbr[14] = 32; vbr[16] = 2;
    put32(vbr + 32, 1000000); put32(vbr + 36, 1024);
    put32(vbr + 44, 2); memcpy(vbr + 82, "FAT32   ", 8);
}
static void part(unsigned i, uint32_t start)
{
    uint8_t *p = mbr + 446 + i * 16;
    p[4] = 0x0c; put32(p + 8, start); put32(p + 12, 1000000);
}
int sdgecko_readSectors(int drive, uint32_t lba, uint32_t count, void *buf)
{
    assert(drive == 2 && count == 1 && ((uintptr_t)buf % 32) == 0);
    if ((!lba && fail0) || (lba == 2048 && failfirst)) return -77;
    memcpy(buf, !lba ? mbr : vbr, 512);
    return 0;
}
bool fatMount(const char *name, const DISC_INTERFACE *disc, sec_t lba,
              uint32_t pages, uint32_t sectors)
{
    assert(!strcmp(name, "sd") && pages == 8 && sectors == 8);
    assert(!(disc->features & FEATURE_MEDIUM_CANWRITE));
    /* Even if libfat tries an FSInfo update, no storage write is forwarded. */
    assert(!disc->writeSectors(1, 1, vbr));
    ++mounts; mounted = lba;
    return mount_ok;
}
static void reset(void)
{
    memset(mbr, 0, 512); mbr[510] = 0x55; mbr[511] = 0xaa;
    fat32(); start_ok = inserted = mount_ok = 1;
    fail0 = failfirst = 0; mounts = 0;
}
int main(void)
{
    reset(); part(0, 2048);
    assert(cd_sd_mount_readonly() == 0 && mounted == 2048 && mounts == 1);
    assert(cd_sd_report.blocked_writes == 1);
    reset(); part(0, 2048); part(1, 2000000); failfirst = 1;
    assert(cd_sd_mount_readonly() == 0 && mounted == 2000000);
    assert(cd_sd_report.read_failed && cd_sd_report.failed_lba == 2048);
    reset(); memcpy(mbr, vbr, 512);
    assert(cd_sd_mount_readonly() == 0 && mounted == 0);
    reset(); fail0 = 1;
    assert(cd_sd_mount_readonly() == -24 && !mounts);
    reset(); mbr[510] = 0;
    assert(cd_sd_mount_readonly() == -25 && !mounts);
    reset(); start_ok = 0;
    assert(cd_sd_mount_readonly() == -20 && !mounts);
    reset(); inserted = 0;
    assert(cd_sd_mount_readonly() == -21 && !mounts);
    reset(); part(0, 2048); mount_ok = 0;
    assert(cd_sd_mount_readonly() == -22 && mounts == 1);
    reset(); part(0, 2048); vbr[13] = 0;
    assert(cd_sd_mount_readonly() == -22 && !mounts);
    fat32(); assert(cd_sd_fat_bpb(vbr, 1000000));
    assert(!cd_sd_fat_bpb(vbr, 999999));
    vbr[13] = 3; assert(!cd_sd_fat_bpb(vbr, 0));
    fat32(); vbr[12] = 4; assert(!cd_sd_fat_bpb(vbr, 0));
    fat32(); put32(vbr + 44, 0); assert(!cd_sd_fat_bpb(vbr, 0));
    fat32(); put32(vbr + 36, 1); assert(!cd_sd_fat_bpb(vbr, 0));
    reset(); part(0, UINT32_MAX - 1);
    uint32_t lba, extent;
    assert(!cd_sd_fat_partition(mbr, 0, &lba, &extent));
    assert(!cd_sd_fat_partition(mbr, 4, &lba, &extent));
    reset(); part(0, 2048); mbr[450] = 0xee;
    assert(!cd_sd_fat_partition(mbr, 0, &lba, &extent));
    puts("SD mount fault matrix passed: read-only boundary, partition scan, BPB bounds, I/O failures.");
}
