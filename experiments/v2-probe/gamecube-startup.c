#include <gccore.h>
#include <ogc/irq.h>
#include <stdio.h>
#include <string.h>
#include "cubedeck/joybus_recovery.h"
#include "startup_fixture.h"

#define PORT 1
#define WATCHDOG 120u
#define SETTLE 3u
#define SYNC_ATTEMPTS 8u
static cdjr_link link;
static volatile uint32_t tick;
static u8 command[32] ATTRIBUTE_ALIGN(32);
static u8 response[32] ATTRIBUTE_ALIGN(32);
/* Immutable owner descriptor until its mailbox has been consumed. libogc
 * has no callback context argument, so at most one owned command exists. */
static volatile uint32_t owned_token, owned_kind, owned_tick;
static volatile unsigned int owned, mailbox_done;
static volatile uint32_t mailbox_token, mailbox_kind, mailbox_result, mailbox_tick;
static unsigned int data_stage, running, passed, target, interrupted;
static uint32_t session_word, last_reply, last_si_result;
static unsigned int data_error;

static uint32_t reply_word(void)
{
    return (uint32_t)response[0] | ((uint32_t)response[1] << 8) |
           ((uint32_t)response[2] << 16) | ((uint32_t)response[3] << 24);
}
static void completed(s32 channel, u32 result)
{
    if (channel != PORT || !owned || mailbox_done) return;
    mailbox_token = owned_token;
    mailbox_kind = owned_kind;
    mailbox_result = result;
    mailbox_tick = tick;
    mailbox_done = 1u; /* Publish last; descriptor remains owned. */
}
static int submit(uint32_t token, uint32_t kind, u8 opcode,
                  uint32_t out_length, uint32_t in_length)
{
    u32 level = IRQ_Disable();
    if (owned) { IRQ_Restore(level); return 0; }
    memset(command, 0, sizeof(command));
    memset(response, 0, sizeof(response));
    command[0] = opcode;
    if (out_length == 5u) {
        for (unsigned int i = 0u; i < 4u; ++i)
            command[i + 1u] = (u8)(session_word >> (i * 8u));
    }
    owned_token = token;
    owned_kind = kind;
    owned_tick = tick;
    owned = 1u;
    mailbox_done = 0u;
    /* False means no callback scheduled: do not wait for a phantom IRQ. */
    u32 accepted = SI_Transfer(PORT, command, out_length, response,
                              in_length, completed, 50);
    if (!accepted) owned = 0u;
    IRQ_Restore(level);
    return accepted != 0u;
}
static int begin_session(void)
{
    /* Never clear or reuse a descriptor just because the UI changed. */
    if (owned || data_error || cdjr_begin(&link, tick) != 0) return 0;
    session_word = CDSR_WORD(link.generation);
    data_stage = data_error = interrupted = 0u;
    running = 1u;
    return 1;
}
static void stop_session(void)
{
    cdjr_interrupt(&link);
    interrupted = 1u;
    running = 0u;
    data_stage = 0u;
    /* Service still runs every frame to drain callback/watchdog. */
}
static void service(void)
{
    u32 level = IRQ_Disable();
    if (mailbox_done) {
        uint32_t token = mailbox_token, kind = mailbox_kind;
        uint32_t result = mailbox_result, when = mailbox_tick;
        last_si_result = result;
        last_reply = reply_word();
        mailbox_done = 0u;
        owned = 0u;
        IRQ_Restore(level);
        if (kind <= CDJR_READ_READY) {
            cdjr_complete(&link, token, result, last_reply == CDSR_READY, when);
        } else if ((uint32_t)(when - owned_tick) >= WATCHDOG) {
            data_error = 1u;
        } else if (running && link.phase == CDJR_READY && !data_error) {
            if (result) data_error = 2u;
            else if (kind == 5u) data_stage = 2u;
            else if (last_reply != (session_word ^ CDSR_REPLY_XOR)) data_error = 3u;
            else {
                data_stage = 3u;
                ++passed;
                running = 0u;
            }
        }
    } else IRQ_Restore(level);

    /* Data timeout is terminal as well; retained owned buffers cannot be
     * recycled by A/Y/B, even if a callback eventually arrives. */
    if (owned && owned_kind > CDJR_READ_READY &&
        (uint32_t)(tick - owned_tick) >= WATCHDOG) data_error = 1u;
    cdjr_action action;
    uint32_t token;
    int poll = cdjr_poll(&link, tick, &action, &token);
    if (poll == 1) {
        u8 opcode = action == CDJR_SEND_RESET ? 0xFF :
                    action == CDJR_SEND_STATUS ? 0x00 : 0x14;
        if (!submit(token, (uint32_t)action, opcode, 1u,
                    action == CDJR_READ_READY ? 5u : 3u))
            cdjr_rejected(&link, token);
    }
    if (link.phase == CDJR_FAILED || data_error) running = 0u;
    if (running && link.phase == CDJR_READY && !owned) {
        if (data_stage == 0u) {
            data_stage = 1u;
            if (!submit(0u, 5u, 0x15, 5u, 1u)) data_error = 4u;
        } else if (data_stage == 2u) {
            if (!submit(0u, 6u, 0x14, 1u, 5u)) data_error = 4u;
        }
    }
    if (!running && !owned && !interrupted && !data_error &&
        link.phase == CDJR_READY && passed < target)
        begin_session();
}
#ifndef CUBEDECK_STARTUP_HOST_TEST
int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    VIDEO_Init();
    PAD_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    if (!mode) return 0;
    void *xfb = SYS_AllocateFramebuffer(mode);
    if (!xfb) return 0;
    xfb = MEM_K0_TO_K1(xfb);
    console_init(xfb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * 2);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (mode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();
    cdjr_init(&link, WATCHDOG);
    cdjr_configure(&link, SETTLE, SYNC_ATTEMPTS);
    u16 previous_buttons = 0u;
    for (;;) {
        /* PAD_ScanPads retries absent pads with PAD_Reset, including the GBA
         * port. Read without that rescan loop to avoid competing type queries.
         * PAD_Init's initial discovery may still make submission fail safely. */
        SI_DisablePolling(0x40000000u);
        PADStatus pads[4];
        PAD_Read(pads);
        u16 buttons = pads[0].err == PAD_ERR_NONE ? pads[0].button : 0u;
        u16 down = buttons & (u16)~previous_buttons;
        previous_buttons = buttons;
        if (down & PAD_BUTTON_B) stop_session();
        if ((down & (PAD_BUTTON_A | PAD_BUTTON_Y)) && !running &&
            !owned && !data_error && link.error != CDJR_ERR_TIMEOUT) {
            passed = 0u;
            target = (down & PAD_BUTTON_Y) ? 16u : 1u;
            begin_session();
        }
        service();
        printf("\x1b[2J\x1b[2;2HCUBEDECK STARTUP EXPERIMENT - UNVALIDATED\n\n");
        printf("  Port 2: RESET > STATUS > ready signature > session echo\n");
        printf("  A: one session   Y: 16 fresh sessions   B: interrupt\n\n");
        printf("  Phase: %u   Error: %u   Data error: %u\n",
               (unsigned int)link.phase, (unsigned int)link.error, data_error);
        printf("  Session: %lu   Ready reads: %u/%u\n",
               (unsigned long)link.generation, link.sync_attempts, SYNC_ATTEMPTS);
        printf("  Completed: %u/%u   Owned SI: %u   Interrupted: %u\n",
               passed, target, owned, interrupted);
        printf("  SI result: %08lX   Reply: %08lX\n",
               (unsigned long)last_si_result, (unsigned long)last_reply);
        if (link.error == CDJR_ERR_TIMEOUT || data_error == 1u)
            printf("\n  Timeout latched. Restart consoles; buffers not reused.\n");
        printf("\n  Use ONLY cubedeck-v2-startup.gba companion.\n");
        printf("  Blue: reset ready. Cyan: session word received.\n");
        printf("  RAM diagnostics only; no cartridge/save/storage I/O.\n");
        VIDEO_WaitVSync();
        ++tick;
    }
}

#endif
