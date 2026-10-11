#ifndef CD_SD_MOUNT_H
#define CD_SD_MOUNT_H
#include <stdint.h>
typedef struct {
    int started, inserted, sector0_ok, signature, superfloppy;
    int mount_ok, mount_errno, read_result, read_failed, failed_result, blocked_writes;
    uint32_t last_read_lba, failed_lba, mounted_lba, reads;
    uint32_t lba[4], extent[4];
    unsigned type[4], status[4];
    unsigned bps, spc, reserved, fats, roots, fat16, jump;
    uint32_t total32, fat32, root_cluster;
    int gpt_read, gpt_signature, gpt_error;
    unsigned gpt_candidates, gpt_mounted_index;
    uint32_t gpt_lba[4], gpt_extent[4];
    unsigned gpt_index[4];
    int gpt_bpb[4], gpt_mount_attempt[4];
    int candidate[4], read_ok[4], bpb_ok[4], mount_attempt[4], fat_label[4];
} cd_sd_mount_report;
extern cd_sd_mount_report cd_sd_report;
int cd_sd_mount_readonly(void);
#endif
