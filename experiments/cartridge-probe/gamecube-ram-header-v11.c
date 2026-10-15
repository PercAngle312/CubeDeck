/* Separate v11 read-only header test. Proven v9/v10 entrypoints untouched. */
#include <gccore.h>
#include <stdio.h>
#include <errno.h>
#include "cubedeck/gba_multiboot_plan.h"
#include "sd_mount.h"
#include "joyboot_upload_v10.h"
#include "ram_v11_link.h"
#include "cubedeck/image_integrity.h"
#include <string.h>
#ifndef CD_V11_BUILD_ID
#define CD_V11_BUILD_ID "local"
#endif
static uint8_t image[CD_MB_MAX_IMAGE_BYTES] ATTRIBUTE_ALIGN(32);
static cd_jbu10 upload;
static cd_v11_link link;
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
    f = fopen("sd:/cubedeck/ram-header-v11.gba", "rb");
    if (!f) { open_errno = errno; return -23; }
    if (fseek(f, 0, SEEK_END)) { fclose(f); return -3; }
    size = ftell(f);
    if (size < (long)CD_MB_MIN_IMAGE_BYTES || size > (long)CD_MB_MAX_IMAGE_BYTES ||
        (size & 15L) || fseek(f, 0, SEEK_SET)) { fclose(f); return -4; }
    loaded = (size_t)size;
    if (fread(image, 1, loaded, f) != loaded) { fclose(f); return -5; }
    fclose(f);
    if (cd_mb_plan_image(image, loaded, &plan) || loaded > 16384u ||
        memcmp(image+0xa0,"CDHEAD11",8) || memcmp(image+0xac,"CD11",4)) return -6;
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
            if (cd_v11_link_start(&link,(uint8_t)frame,frame)) storage_error=-31;
            else link_started=1u;
        }
        if (link_started) (void)cd_v11_link_step(&link,frame);
        printf("\x1b[2J\x1b[2;2HCubeDeck v11 CART HEADER  build %s\n\n",CD_V11_BUILD_ID);
        printf("SD: driver %d card %d mount %d error %d errno %d\n",
            cd_sd_report.started,cd_sd_report.inserted,cd_sd_report.mount_ok,
            storage_error,open_errno);
        printf("GPT error %d entry %u LBA %lu  GBA image %lu bytes\n\n",
            cd_sd_report.gpt_error,cd_sd_report.gpt_mounted_index,
            (unsigned long)cd_sd_report.mounted_lba,(unsigned long)loaded);
        printf("UPLOAD: %s   err %u failed phase %u\n",phase_name(upload.phase),
            upload.error,upload.failed_phase);
        printf("Payload %lu/%lu  pending %u  SI active %lu/%lu\n",
            (unsigned long)upload.sender.words.offset,(unsigned long)loaded,
            upload.awaiting,(unsigned long)upload.ready.si.active,(unsigned long)upload.si.active);
        printf("READY: phase %u err %u result %08lX reply %08lX\n",
            upload.ready.phase,upload.ready.error,(unsigned long)upload.ready.last_result,
            (unsigned long)upload.ready.last_reply);
        printf("READY bytes %02X %02X %02X %02X  RAM %08lX\n\n",
            upload.ready.si.response[0],upload.ready.si.response[1],
            upload.ready.si.response[2],upload.ready.si.response[3],(unsigned long)upload.last_reply);
        printf("HEADER: stage %u bytes %u/192 nonce %02X err %u\n",
            link.stage,link.client.offset,link.client.nonce,link.error);
        printf("SI result %08lX reply %08lX pending %u active %lu\n",
            (unsigned long)link.last_result,(unsigned long)link.last_reply,
            link.awaiting,(unsigned long)link.si.active);
        printf("SI token %lu reads %u refused %u\n\n",
            (unsigned long)link.token,link.reads,link.refusals);
        if (storage_error) printf("FILE ERROR: v11 template + your own gba-bios.bin required.\n");
        else if (upload.phase==CD_JBU_IDLE) {
            printf("First pass v11 RAM PREFLIGHT 3/3 WITHOUT cartridge.\n");
            printf("Retail cart in GBA BEFORE power-on; hold START + SELECT.\n");
            printf("Port 2, BIOS receive mode. Press GC A ONCE to scan.\n");
        } else if (upload.phase==CD_JBU_FAILED || link.stage==CD_V11_LINK_FAILED)
            printf("TERMINAL FAILURE: photograph screen; reboot BOTH systems.\n");
        else if (link.stage==CD_V11_LINK_PASS) {
            printf("PASS: 192-byte header checksum + fixed byte valid.\n");
            printf("Title: %s  Code: %s\n",link.client.parsed.title,link.client.parsed.game_code);
            printf("Maker: %s  Version: %u\n",link.client.parsed.maker_code,
                link.client.parsed.software_version);
        } else printf("RUNNING: wait for GC PASS; green GBA means bytes sent.\n");
        printf("\nExperimental. ROM header only; no save access or SD writes.\n");
        VIDEO_WaitVSync();
        ++frame;
    }
}
