/* Isolated GameCube multiboot discovery probe.
 * This deliberately does NOT send a multiboot image or cartridge command.
 * It exercises bounded SI device discovery before any bootstrap work. */
#include <gccore.h>
#include <ogc/irq.h>
#include <stdio.h>
#include <stdint.h>

#define GBA_PORT 1
#define SI_TIMEOUT_FRAMES 120u
static u8 command[32] ATTRIBUTE_ALIGN(32);
static u8 reply[32] ATTRIBUTE_ALIGN(32);
static volatile unsigned int in_flight, done;
static volatile u32 callback_result, completed_frames;
static volatile u32 frames;
static unsigned int attempts, status, failed;
static u32 last_reply;

static void completed(s32 channel, u32 result)
{
    if (channel != GBA_PORT || !in_flight || done) return;
    callback_result = result;
    completed_frames = frames;
    done = 1u;
}

static int submit(u8 opcode, u32 tx_bytes, u32 rx_bytes)
{
    u32 level = IRQ_Disable();
    if (in_flight) { IRQ_Restore(level); return 0; }
    for (unsigned i = 0; i < sizeof(command); ++i) command[i] = 0u;
    for (unsigned i = 0; i < sizeof(reply); ++i) reply[i] = 0u;
    command[0] = opcode;
    done = 0u;
    in_flight = 1u;
    u32 accepted = SI_Transfer(GBA_PORT, command, tx_bytes, reply,
                               rx_bytes, completed, 50);
    if (!accepted) in_flight = 0u;
    IRQ_Restore(level);
    return accepted != 0u;
}

int main(void)
{
    VIDEO_Init();
    PAD_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    if (!mode) return 0;
    void *xfb = SYS_AllocateFramebuffer(mode);
    if (!xfb) return 0;
    xfb = MEM_K0_TO_K1(xfb);
    console_init(xfb, 20, 20, mode->fbWidth, mode->xfbHeight,
                 mode->fbWidth * 2);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    unsigned short previous = 0u;
    for (;;) {
        SI_DisablePolling(0x40000000u);
        PADStatus pads[4];
        PAD_Read(pads);
        unsigned short keys = pads[0].err == PAD_ERR_NONE ? pads[0].button : 0u;
        unsigned short pressed = keys & (unsigned short)~previous;
        previous = keys;
        if ((pressed & PAD_BUTTON_A) && !in_flight && !failed) {
            attempts = 0u;
            status = 0u;
            if (!submit(0xFFu, 1u, 3u)) failed = 1u;
            else { attempts = 1u; status = 1u; }
        }
        if (pressed & PAD_BUTTON_B) {
            /* Aborting never cancels SI or reuses its buffers. */
            failed = 1u;
        }
        if (done) {
            u32 level = IRQ_Disable();
            u32 result = callback_result, when = completed_frames;
            done = 0u;
            in_flight = 0u;
            last_reply = (u32)reply[0] | ((u32)reply[1] << 8) |
                         ((u32)reply[2] << 16) | ((u32)reply[3] << 24);
            IRQ_Restore(level);
            if (result || (u32)(when - (frames - 1u)) >= SI_TIMEOUT_FRAMES)
                failed = 1u;
            else if (!failed && status == 1u) {
                if (!submit(0x00u, 1u, 3u)) failed = 1u;
                else { status = 2u; ++attempts; }
            } else if (!failed && status == 2u) status = 3u;
        }
        if (in_flight && (u32)(frames - completed_frames) >= SI_TIMEOUT_FRAMES)
            failed = 1u; /* terminal until relaunch if callback never arrives */
        printf("\x1b[2J\x1b[2;2HCUBEDECK MULTIBOOT DISCOVERY (NO UPLOAD)\n\n");
        printf("  A: RESET + STATUS probe; B: stop (terminal)\n");
        printf("  Stage: %u  Attempts: %u  Failed: %u  SI owned: %u\n",
               status, attempts, failed, in_flight);
        printf("  Last reply: %08lX\n", (unsigned long)last_reply);
        printf("  No ROM, multiboot, save, or storage writes.\n");
        VIDEO_WaitVSync();
        ++frames;
    }
}
