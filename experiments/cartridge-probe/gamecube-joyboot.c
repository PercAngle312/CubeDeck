/* Experimental CubeDeck Joyboot loader; separate from proven diagnostics.
 * Loads a user-supplied multiboot probe from SD, never writes SD or saves.
 * No commercial ROMs, BIOS or copyrighted data included. */
#include <gccore.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cubedeck/gba_multiboot_plan.h"
#include "joyboot_handshake.h"
#include "joyboot_bridge.h"
#include "probe_receiver.h"
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
static int load_image(void)
{
    FILE *f;
    cd_mb_plan plan;
    long size;
    /* SD2SP2 is the GameCube serial port 2 slot. Explicitly mount it as
     * "sd" rather than assuming a launcher-provided default FAT volume. */
    if (!fatMountSimple("sd", &__io_gcsd2)) {
        sd_mount_status = -1;
        /* A launcher may have already registered the same volume.
         * In that case a read-only open is a better test than failing
         * simply because mounting again did not succeed. */
    } else {
        sd_mount_status = 1;
    }
    f = fopen("sd:/cubedeck/joyboot-probe.gba", "rb");
    if (!f) return sd_mount_status < 0 ? -1 : -2;
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
            if (cd_jb_handshake_init(&handshake, PORT, loaded) != 0)
                { state = -1; error = -7; }
            else state = 1;
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
        printf("\x1b[2J\x1b[2;2HCubeDeck EXPERIMENTAL JOYBOOT\n\n");
        printf("Source: sd:/cubedeck/joyboot-probe.gba\n");
        printf("SD2SP2 mount: %s\n", sd_mount_status > 0 ? "OK" : "failed/already mounted");
        printf("Image bytes: %lu   State: %d   Error: %d\n",
               (unsigned long)loaded, state, error);
        if (state == 0) printf("Press A to try upload to GBA port 2.\n");
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
