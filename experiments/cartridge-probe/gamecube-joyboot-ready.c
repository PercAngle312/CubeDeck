/* v8 BIOS readiness only. Keep the hardware-verified v7 DOL unchanged.
 * Reads the existing storage fixture; no payload or cartridge commands. */
#include <gccore.h>
#include <stdio.h>
#include <errno.h>
#include "cubedeck/gba_multiboot_plan.h"
#include "sd_mount.h"
#include "joyboot_ready.h"
#define TIMEOUT_FRAMES 180u
static uint8_t image[CD_MB_MAX_IMAGE_BYTES] ATTRIBUTE_ALIGN(32);
static cd_jbr ready;
static size_t loaded;
static int open_errno;
static int load_fixture(void)
{
    FILE *f;
    long size;
    cd_mb_plan plan;
    int result = cd_sd_mount_readonly();
    if (result) return result;
    errno = 0;
    f = fopen("sd:/cubedeck/joyboot-probe.gba", "rb");
    if (!f) { open_errno = errno; return -23; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return -3; }
    size = ftell(f);
    if (size < (long)CD_MB_MIN_IMAGE_BYTES || size > (long)CD_MB_MAX_IMAGE_BYTES ||
        (size & 15L) || fseek(f, 0, SEEK_SET)) { fclose(f); return -4; }
    loaded = (size_t)size;
    if (fread(image, 1, loaded, f) != loaded) { fclose(f); return -5; }
    fclose(f);
    return cd_mb_plan_image(image, loaded, &plan) ? -6 : 0;
}
static const char *phase_name(unsigned phase)
{
    switch (phase) {
    case CD_JBR_IDLE: return "WAITING FOR A";
    case CD_JBR_RESET: return "RESET";
    case CD_JBR_STATUS: return "STATUS / BOOT READY";
    case CD_JBR_KEY: return "SESSION KEY READ";
    case CD_JBR_READY: return "READY";
    default: return "FAILED";
    }
}
int main(void)
{
    GXRModeObj *mode;
    void *xfb;
    uint32_t frame = 0;
    int storage_error;
    VIDEO_Init();
    PAD_Init();
    mode = VIDEO_GetPreferredMode(NULL);
    if (!mode) return 0;
    xfb = SYS_AllocateFramebuffer(mode);
    if (!xfb) return 0;
    xfb = MEM_K0_TO_K1(xfb);
    console_init(xfb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * 2);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    storage_error = load_fixture();
    for (;;) {
        PAD_ScanPads();
        if (!storage_error && ready.phase == CD_JBR_IDLE &&
            (PAD_ButtonsDown(0) & PAD_BUTTON_A))
            (void)cd_jbr_start(&ready, 1u, frame, TIMEOUT_FRAMES);
        if (ready.phase != CD_JBR_IDLE)
            (void)cd_jbr_step(&ready, frame, TIMEOUT_FRAMES);
        printf("\x1b[2J\x1b[2;2HCubeDeck Joyboot READINESS v8\n\n");
        printf("Storage: driver %d card %d mount %d error %d errno %d\n",
               cd_sd_report.started, cd_sd_report.inserted,
               cd_sd_report.mount_ok, storage_error, open_errno);
        printf("File: sd:/cubedeck/joyboot-probe.gba (%lu bytes)\n",
               (unsigned long)loaded);
        printf("GPT error %d mounted entry %u LBA %lu\n\n",
               cd_sd_report.gpt_error, cd_sd_report.gpt_mounted_index,
               (unsigned long)cd_sd_report.mounted_lba);
        printf("GBA: port 2   Stage: %s\n", phase_name(ready.phase));
        printf("STATUS polls %u   SI refusals %u\n",
               ready.status_polls, ready.refused);
        if (ready.error) printf("Error %u at %s\n",
                                 ready.error, phase_name(ready.failed_phase));
        printf("SI raw %08lX reply %08lX pending %lu callback %lu\n",
               (unsigned long)ready.last_result, (unsigned long)ready.last_reply,
               (unsigned long)ready.si.active, (unsigned long)ready.si.done);
        if (storage_error) printf("Storage failed. Keep v7 and report this screen.\n");
        else if (ready.phase == CD_JBR_IDLE) {
            printf("Turn GBA on with no cartridge; wait at boot screen.\n");
            printf("Press A once to check BIOS readiness.\n");
        } else if (ready.phase == CD_JBR_READY)
            printf("PASS: BIOS ready and session key received.\n");
        else if (ready.phase == CD_JBR_FAILED)
            printf("Terminal failure. Photograph screen; reboot to retry.\n");
        printf("\nNO PAYLOAD SENT. GBA program execution is not tested.\n");
        printf("No cartridge access. SD reads only. No storage writes.\n");
        VIDEO_WaitVSync();
        ++frame;
    }
}
