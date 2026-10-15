/* Separate v10 RAM-only ping test. Known-good v9 source is untouched. */
#include <gccore.h>
#include <stdio.h>
#include <errno.h>
#include "cubedeck/gba_multiboot_plan.h"
#include "sd_mount.h"
#include "joyboot_upload_v10.h"
#include "ram_v10_link.h"
#include "cubedeck/image_integrity.h"
#include <string.h>
#define TIMEOUT_FRAMES 180u
static uint8_t image[CD_MB_MAX_IMAGE_BYTES] ATTRIBUTE_ALIGN(32);
static cd_jbu10 upload;
static cd_v10_link link;
static unsigned link_started;
static uint8_t bios[16384];
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
    f = fopen("sd:/cubedeck/ram-ping-v10.gba", "rb");
    if (!f) { open_errno = errno; return -23; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return -3; }
    size = ftell(f);
    if (size < (long)CD_MB_MIN_IMAGE_BYTES || size > (long)CD_MB_MAX_IMAGE_BYTES ||
        (size & 15L) || fseek(f, 0, SEEK_SET)) { fclose(f); return -4; }
    loaded = (size_t)size;
    if (fread(image, 1, loaded, f) != loaded) { fclose(f); return -5; }
    fclose(f);
    if (cd_mb_plan_image(image, loaded, &plan) || loaded > 16384u ||
        memcmp(image+0xa0,"CDPING10",8)) return -6;
    for (unsigned i=4;i<0xa0;++i) if (image[i]) return -27;
    /* The ZIP has no BIOS/logo. Read the user's own dump and patch RAM only.
     * This specific BIOS revision is pinned by its full-file CRC, not guessed. */
    f=fopen("sd:/cubedeck/gba-bios.bin","rb");
    if (!f) { open_errno=errno; return -28; }
    size_t got=fread(bios,1,sizeof(bios),f);
    int extra=fgetc(f);
    int bad=ferror(f); fclose(f);
    if (got!=sizeof(bios) || extra!=EOF || bad ||
        cdim_crc32(bios,sizeof(bios))!=0x81977335u) return -29;
    memcpy(image+4,bios+0x3290,156);
    memset(bios,0,sizeof(bios));
    return 0;
}
static const char *phase_name(unsigned phase) {
    static const char *names[]={"WAITING FOR A","BIOS READY","CLIENT KEY",
      "PAYLOAD / CRC","BIOS FINAL READ","WAIT FOR RAM PROGRAM",
      "RAM STATUS READ","RAM PROGRAM PASS","FAILED"};
    return phase<=CD_JBU_FAILED ? names[phase] : "INVALID";
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
        /* Experiment: after the launch button, do not poll controllers while
         * the GBA SI channel is performing RESET/STATUS/READ/UPLOAD.
         * The first SI transaction starts on the next video frame so the
         * last PAD scan and GBA RESET do not happen in the same iteration. */
        if (upload.phase==CD_JBU_IDLE) {
            PAD_ScanPads();
            if (!storage_error && (PAD_ButtonsDown(0)&PAD_BUTTON_A))
                if (cd_jbu10_start(&upload,image,loaded,frame)) storage_error=-30;
        } else (void)cd_jbu10_step(&upload,frame);
        if (upload.phase==CD_JBU_PASS && !link_started) {
            if (cd_v10_link_start(&link,0x1234u,frame)) storage_error=-31;
            else link_started=1u;
        }
        if (link_started) (void)cd_v10_link_step(&link,frame);
        printf("\x1b[2J\x1b[2;2HCubeDeck v10 TWO-WAY PING / ACK\n\n");
        printf("Storage: driver %d card %d mount %d error %d errno %d\n",
               cd_sd_report.started, cd_sd_report.inserted,
               cd_sd_report.mount_ok, storage_error, open_errno);
        printf("File: sd:/cubedeck/ram-ping-v10.gba (%lu bytes)\n",
               (unsigned long)loaded);
        printf("GPT error %d mounted entry %u LBA %lu\n\n",
               cd_sd_report.gpt_error, cd_sd_report.gpt_mounted_index,
               (unsigned long)cd_sd_report.mounted_lba);
        printf("GBA port 2: %s\n",phase_name(upload.phase));
        printf("Payload %lu/%lu bytes, refused %u, runtime reads %u\n",
          (unsigned long)upload.sender.words.offset,(unsigned long)loaded,
          upload.refused,upload.runtime_reads);
        printf("Error %u at %s, pending %lu/%lu\n",upload.error,
          phase_name(upload.failed_phase),(unsigned long)upload.ready.si.active,
          (unsigned long)upload.si.active);
        printf("Ready: phase %u failed %u error %u polls %u refused %u\n",
          upload.ready.phase, upload.ready.failed_phase, upload.ready.error,
          upload.ready.status_polls, upload.ready.refused);
        printf("Ready SI: result %08lX reply %08lX awaiting %u active %u\n",
          (unsigned long)upload.ready.last_result,
          (unsigned long)upload.ready.last_reply,
          upload.ready.awaiting, upload.ready.si.active);
        printf("Ready SI bytes: %02X %02X %02X %02X age %lu frames\n",
          upload.ready.si.response[0], upload.ready.si.response[1],
          upload.ready.si.response[2], upload.ready.si.response[3],
          (unsigned long)(frame - upload.ready.si.submitted_frame));
        printf("Ready SI state: callback %lu token %lu\n",
          (unsigned long)upload.ready.si.done,
          (unsigned long)upload.ready.si.token);
        printf("BIOS reply %08lX (untrusted), RAM reply %08lX\n",
          (unsigned long)upload.bios_reply,(unsigned long)upload.last_reply);
        if (storage_error) printf("File error. Need v9 template and your own gba-bios.bin.\n");
        else if (upload.phase==CD_JBU_IDLE)
          printf("GBA on, NO cartridge. Press A once to upload RAM program.\n");
        else if (upload.phase==CD_JBU_PASS)
          printf("RAM v10 READY received. Waiting for sequence ACK.\n");
        else if (upload.phase==CD_JBU_FAILED)
          printf("Terminal failure. Photograph screen; reboot both to retry.\n");
        printf("PING stage %u seq %04X last %08lX err %u reads %u\n",
               (unsigned)link.client.stage,(unsigned)link.client.sequence,
               (unsigned long)link.client.last_reply,link.error,link.reads);
        printf("PING SI result %08lX pending %u active %lu done %lu refused %u\n",
               (unsigned long)link.last_result,link.awaiting,
               (unsigned long)link.si.active,(unsigned long)link.si.done,
               link.refusals);
        if (link.client.stage==CD_V10_CLIENT_PASS)
            printf("PASS: GBA RAM PING / ACK matched. GBA PURPLE.\n");
        printf("No cartridge access. SD reads only. No storage writes.\n");
        VIDEO_WaitVSync();
        ++frame;
    }
}
