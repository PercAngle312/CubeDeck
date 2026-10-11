#include <gccore.h>
#include <fat.h>
#include <errno.h>
#include <sdcard/gcsd.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <string.h>
#include "sd_mount.h"
#include "sd_fat_volume.h"
cd_sd_mount_report cd_sd_report;
static DISC_INTERFACE readonly_sd;
static uint8_t mbr[512] ATTRIBUTE_ALIGN(32);
static uint8_t boot[512] ATTRIBUTE_ALIGN(32);
static bool read_sectors(sec_t lba, sec_t count, void *buffer)
{
    int result = sdgecko_readSectors(2, lba, count, buffer);
    ++cd_sd_report.reads;
    cd_sd_report.last_read_lba = lba;
    cd_sd_report.read_result = result;
    if (result != CARDIO_ERROR_READY) {
        cd_sd_report.read_failed = 1;
        cd_sd_report.failed_lba = lba;
        cd_sd_report.failed_result = result;
    }
    return result == CARDIO_ERROR_READY;
}
static bool deny_write(sec_t lba, sec_t count, const void *buffer)
{
    (void)lba; (void)count; (void)buffer;
    ++cd_sd_report.blocked_writes;
    return false;
}
static int mount_at(uint32_t lba)
{
    int result;
    errno = 0;
    /* Explicit volume offset; bounded page prefetch in libdvm/FatFs. */
    result = fatMount("sd", &readonly_sd, lba, 8, 8);
    cd_sd_report.mount_errno = errno;
    if (result) {
        cd_sd_report.mount_ok = 1;
        cd_sd_report.mounted_lba = lba;
    }
    return result;
}
int cd_sd_mount_readonly(void)
{
    unsigned i;
    memset(&cd_sd_report, 0, sizeof(cd_sd_report));
    readonly_sd = __io_gcsd2;
    readonly_sd.features &= ~FEATURE_MEDIUM_CANWRITE;
    readonly_sd.writeSectors = deny_write;
    readonly_sd.readSectors = read_sectors;
    cd_sd_report.started = readonly_sd.startup();
    if (!cd_sd_report.started) return -20;
    cd_sd_report.inserted = readonly_sd.isInserted();
    if (!cd_sd_report.inserted) return -21;
    cd_sd_report.sector0_ok = read_sectors(0, 1, mbr);
    if (!cd_sd_report.sector0_ok) return -24;
    cd_sd_report.signature = mbr[510] == 0x55 && mbr[511] == 0xaa;
    if (!cd_sd_report.signature) return -25;
    cd_sd_report.superfloppy = cd_sd_fat_bpb(mbr, 0);
    if (cd_sd_report.superfloppy) return mount_at(0) ? 0 : -22;
    for (i = 0; i < 4; ++i) {
        cd_sd_report.candidate[i] = cd_sd_fat_partition(mbr, i,
            &cd_sd_report.lba[i], &cd_sd_report.extent[i]);
        if (!cd_sd_report.candidate[i]) continue;
        cd_sd_report.read_ok[i] = read_sectors(cd_sd_report.lba[i], 1, boot);
        if (!cd_sd_report.read_ok[i]) continue;
        cd_sd_report.bpb_ok[i] = cd_sd_fat_bpb(boot, cd_sd_report.extent[i]);
        cd_sd_report.fat_label[i] = !memcmp(boot + 54, "FAT", 3) ||
                                     !memcmp(boot + 82, "FAT", 3);
        if (!cd_sd_report.bpb_ok[i]) continue;
        cd_sd_report.mount_attempt[i] = 1;
        if (mount_at(cd_sd_report.lba[i])) return 0;
    }
    return -22;
}
