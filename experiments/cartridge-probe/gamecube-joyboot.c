/* Experimental CubeDeck Joyboot loader; separate from proven diagnostics.
 * Loads a user-supplied multiboot probe from SD, never writes SD or saves.
 * No commercial ROMs, BIOS or copyrighted data included. */
#include <gccore.h>
#include <fat.h>
#include <errno.h>
#include <sdcard/gcsd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cubedeck/gba_multiboot_plan.h"
#include "joyboot_handshake.h"
#include "joyboot_bridge.h"
#include "probe_receiver.h"
#include "sd_mount.h"
#define PORT 1u
#define MAX_IMAGE 0x3ff40u
#define TIMEOUT_FRAMES 180u
static uint8_t image[MAX_IMAGE] ATTRIBUTE_ALIGN(32);
static cd_jb_handshake handshake;
static cd_jb_bridge bridge;
static cd_probe_receiver probe_receiver;
static uint32_t frames;
static int state, error;
static uint32_t final_response;
static uint32_t probe_start_frame;
static size_t loaded;
static int sd_mount_status;
static int sd_device_started, sd_card_detected, sd_open_errno;
static int load_image(void)
{
#ifdef CUBEDECK_EMBED_PROBE
    extern const unsigned char cd_embedded_probe[];
    extern const unsigned int cd_embedded_probe_size;
    cd_mb_plan plan;
    if (cd_embedded_probe_size < 0x200u ||
        cd_embedded_probe_size > MAX_IMAGE ||
        (cd_embedded_probe_size & 15u)) return -4;
    loaded = cd_embedded_probe_size;
    memcpy(image, cd_embedded_probe, loaded);
    return cd_mb_plan_image(image, loaded, &plan) == 0 ? 0 : -6;
#else
    FILE *f;
    cd_mb_plan plan;
    long size;
    int mount_result = cd_sd_mount_readonly();
    sd_device_started = cd_sd_report.started;
    sd_card_detected = cd_sd_report.inserted;
    sd_mount_status = cd_sd_report.mount_ok;
    if (mount_result) return mount_result;
    errno = 0;
    f = fopen("sd:/cubedeck/joyboot-probe.gba", "rb");
    if (!f) {
        sd_open_errno = errno;
        return -23;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -3; }
    size = ftell(f);
    if (size < 0x200L || size > (long)MAX_IMAGE ||
        (size & 15L) || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f); return -4;
    }
    loaded = (size_t)size;
    if (fread(image, 1, loaded, f) != loaded) {
        fclose(f); return -5;
    }
    fclose(f);
    if (cd_mb_plan_image(image, loaded, &plan) != 0) return -6;
    return 0;
#endif
}
int main(void)
{
    GXRModeObj *mode;
    void *xfb;
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
    error = load_image();
    if (error) state = -1;
    for (;;) {
        PAD_ScanPads();
        if (state == 0 && (PAD_ButtonsDown(0) & PAD_BUTTON_A)) {
#ifdef CUBEDECK_SD_ONLY
            /* Storage milestone: never start SI upload or a cartridge probe. */
            (void)0;
#else
            if (cd_jb_handshake_init(&handshake, PORT, loaded) != 0)
                { state = -1; error = -7; }
            else state = 1;
#endif
        }
        if (state == 1) {
            int status = cd_jb_handshake_step(&handshake, frames, TIMEOUT_FRAMES);
            if (status < 0) { state = -1; error = -8; }
            else if (status == 1) {
                if (cd_jb_bridge_init(&bridge, PORT, image, loaded,
                                       handshake.raw_session_key) != 0)
                    { state = -1; error = -9; }
                else state = 2;
            }
        } else if (state == 2) {
            int status = cd_jb_bridge_step(&bridge, frames, TIMEOUT_FRAMES);
            if (status < 0) { state = -1; error = -10; }
            else if (status == 1) {
                final_response = bridge.final_reply_word;
                /* A completed read does not prove program execution.
                 * Do not announce success pending BIOS response validation. */
                probe_start_frame = frames;
                state = 3;
            }
        }
        if (state == 3 && (uint32_t)(frames - probe_start_frame) >= 60u) {
            int probe_status = cd_probe_receiver_step(&probe_receiver, PORT,
                                                       frames, TIMEOUT_FRAMES);
            if (probe_status < 0) { state = -1; error = -11; }
            else if (probe_status == 1) state = 4;
        }
        printf("\x1b[2J\x1b[2;2HCubeDeck SD2SP2 DIAGNOSTIC v5\n\n");
        #ifdef CUBEDECK_EMBED_PROBE
        printf("Source: EMBEDDED GBA probe (no SD mount needed)\n");
#else
        printf("Source: sd:/cubedeck/joyboot-probe.gba\n");
#endif
        printf("SD2SP2: driver %d card %d mount %d errno %d\n",
               sd_device_started, sd_card_detected, sd_mount_status, sd_open_errno);
        printf("Image bytes: %lu   State: %d   Error: %d\n",
               (unsigned long)loaded, state, error);
        printf("Sector0 %d sig %d raw %d fail %d@%lu\n",
               cd_sd_report.sector0_ok, cd_sd_report.signature,
               cd_sd_report.failed_result, cd_sd_report.read_failed,
               (unsigned long)cd_sd_report.failed_lba);
        printf("FAT errno %d start %lu reads %lu writes blocked %d\n",
               cd_sd_report.mount_errno, (unsigned long)cd_sd_report.mounted_lba,
               (unsigned long)cd_sd_report.reads, cd_sd_report.blocked_writes);
        for (unsigned i = 0; i < 4; ++i)
            printf("P%u LBA %lu candidate %d read %d BPB %d tag %d mount %d\n", i + 1,
                   (unsigned long)cd_sd_report.lba[i], cd_sd_report.candidate[i],
                   cd_sd_report.read_ok[i], cd_sd_report.bpb_ok[i],
                   cd_sd_report.fat_label[i], cd_sd_report.mount_attempt[i]);
#ifdef CUBEDECK_SD_ONLY
        printf("Storage-only v5: upload disabled.\n");
        if (state == 0) printf("PASS: probe file read and image checks passed.\n");
#else
        if (state == 0) printf("Press A to try upload to GBA port 2.\n");
#endif
        if (state == 1) printf("BIOS step: %u  error: %lu\n",
                               handshake.step, (unsigned long)handshake.errors);
        if (state == 2) printf("Payload: %lu / %lu bytes\n",
                               (unsigned long)bridge.sender.words.offset,
                               (unsigned long)bridge.sender.words.padded_length);
        if (state == 3) printf("Final BIOS raw reply: %08lX (UNVERIFIED)\n",
                               (unsigned long)final_response);
        if (state == 4) printf("GBA cartridge checksum: %s (status %08lX)\n",
                               probe_receiver.valid ? "VALID" : "INVALID",
                               (unsigned long)probe_receiver.raw);
        if (state == -1) printf("Terminal failure. Restart experiment to retry.\n");
        printf("Read-only SD. No save operations. No retail ROMs bundled.\n");
        VIDEO_WaitVSync();
        ++frames;
    }
}
