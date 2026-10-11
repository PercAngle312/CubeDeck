#ifndef CD_SD_MOUNT_H
#define CD_SD_MOUNT_H
#include <stdint.h>
typedef struct {
    int started, inserted, sector0_ok, signature, superfloppy;
    int mount_ok, mount_errno, read_result, read_failed, failed_result, blocked_writes;
    uint32_t last_read_lba, failed_lba, mounted_lba, reads;
    uint32_t lba[4], extent[4];
    int candidate[4], read_ok[4], bpb_ok[4], mount_attempt[4], fat_label[4];
} cd_sd_mount_report;
extern cd_sd_mount_report cd_sd_report;
int cd_sd_mount_readonly(void);
#endif
