#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cubedeck/session_guard.h"
#include "ack_control.h"

#define MENU_COUNT 5
#define LINK_QUERY_INTERVAL 60
/* Separate, unverified startup-sync experiment. */
#define CDAP_STARTUP_GAP_VSYNCS 8u

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_SYSTEM,
    SCREEN_CONTROLLER,
    SCREEN_LINK,
    SCREEN_ABOUT,
    SCREEN_EXIT
} Screen;

static GXRModeObj *video_mode;
static u32 frame_count;
static int selected = 0;
static Screen screen = SCREEN_HOME;
static volatile int link_query_pending = 0;
static volatile int link_status = 0; /* 0=not queried, 1=checking, 2=GBA found, 3=not found */
static volatile u32 link_type_value = 0;
static u32 link_last_query_frame = 0;

/* Callback-driven, non-blocking Joybus probe buffers and state. */
static u8 joybus_cmd[32] ATTRIBUTE_ALIGN(32);
static u8 joybus_res[32] ATTRIBUTE_ALIGN(32);
static volatile int joybus_pending = 0;
static volatile int joybus_callback_done = 0;
static volatile u32 joybus_callback_result = 0;
static int joybus_stage = 0; /* 0=idle, 1=RESET, 2=STATUS, 3=SEND, 4=READ, 5=pass, 6=mismatch, 7=transfer fail */
static u32 joybus_stage_frame = 0;
static u8 joybus_status[3] = {0, 0, 0};
static u32 joybus_reply = 0;
static u32 joybus_test_word = 0x12345678u;
static int joybus_round = 0;
static int joybus_rounds_target = 1;
static int joybus_test_mode = 0; /* 1=single, 2=stress, 3=framed packet */
/* Fixed, nonzero test ID shared with the separate v2 GBA probe. */
#define CDV2_PROBE_ID 0x42CD2026u
static uint32_t joybus_packet_words[CDTP_MAX_PACKET_WORDS];
static size_t joybus_packet_word_count = 0;
static size_t joybus_packet_word_index = 0;
static unsigned int v2_frame_index = 0u;
#define V2_FRAME_COUNT 3u
#define V2_FRAME_GAP_VSYNCS 12u
static u32 v2_resume_frame = 0u;
static unsigned int ack_retries = 0u;
static unsigned int ack_recoveries = 0u;
#define CDAP_MAX_RETRIES 2u
/* Failure snapshot: diagnostics only, never initiate another SI transfer
 * after a callback failure or a pending-transfer watchdog expiry. */
enum {
    CDAP_FAIL_NONE = 0, CDAP_FAIL_WATCHDOG = 1,
    CDAP_FAIL_SI_CALLBACK = 2, CDAP_FAIL_RETRY_LIMIT = 3,
    CDAP_FAIL_BUILD = 4
};
static unsigned int failure_reason = CDAP_FAIL_NONE;
static int failure_stage = 0;
static u32 failure_callback_result = 0u;
static u32 failure_reply = 0u;

static void fail_probe(unsigned int reason)
{
    failure_reason = reason;
    failure_stage = joybus_stage;
    failure_callback_result = joybus_callback_result;
    failure_reply = joybus_reply;
    joybus_stage = 7;
}


static int build_v2_frame(unsigned int index)
{
    static const uint8_t sample[7] = {0x43u,0x44u,0x42u,0x4Bu,0x10u,0x20u,0x30u};
    cdgu_message packet;
    unsigned int i;
    if (index >= V2_FRAME_COUNT) return -1;
    memset(&packet, 0, sizeof(packet));
    packet.session_id = CDV2_PROBE_ID;
    packet.opcode = 0x01u;
    packet.sequence = (uint16_t)(index + 1u);
    packet.payload_length = 7u;
    for (i = 0u; i < 7u; ++i)
        packet.payload[i] = (uint8_t)(sample[i] ^ (uint8_t)(index * 17u));
    joybus_packet_word_index = 0u;
    return cdgu_encode(&packet, joybus_packet_words,
                       CDTP_MAX_PACKET_WORDS, &joybus_packet_word_count);
}
#define JOYBUS_TEST_WORD_BASE 0x12345678u
#define JOYBUS_REPLY_XOR 0xA55A3CC3u
#define JOYBUS_STRESS_ROUNDS 16
#define JOYBUS_TIMEOUT_FRAMES 120

static const char *menu_items[MENU_COUNT] = {
    "System information",
    "Controller test",
    "GBA link cable check (Port 2)",
    "About CubeDeck",
    "Exit (use Swiss reset)"
};

static void link_type_callback(s32 channel, u32 type)
{
    (void)channel;
    link_type_value = type;
    link_status = (type & SI_GBA) ? 2 : 3;
    link_query_pending = 0;
}

static void draw_header(void)
{
    printf("\x1b[2;2HCUBEDECK V2  |  ACK SYNC EXPERIMENT\n");
    printf("\x1b[3;2H========================================\n");
}

static void draw_home(void)
{
    draw_header();
    printf("\x1b[5;2HWelcome to CubeDeck\n");
    printf("\x1b[6;2HA lightweight homebrew utility dashboard.\n");
    printf("\x1b[8;2HMAIN MENU\n\n");

    for (int i = 0; i < MENU_COUNT; ++i) {
        printf("\x1b[%d;4H%s %s\n", 10 + i * 2,
               (i == selected) ? ">" : " ", menu_items[i]);
    }

    printf("\x1b[21;2HCONTROLS\n");
    printf("\x1b[22;2HD-Pad: Move    A: Select    B: Back\n");
    printf("\x1b[23;2HV2 EXPERIMENT ONLY  |  No ROM/save writes\n");
}

static void draw_system(void)
{
    draw_header();
    printf("\x1b[5;2HSYSTEM INFORMATION\n");
    printf("\x1b[7;2HConsole target : Nintendo GameCube\n");
    printf("\x1b[8;2HCPU family     : IBM PowerPC 750 (Gekko)\n");
    printf("\x1b[9;2HVideo mode     : libogc preferred mode\n");
    printf("\x1b[10;2HApplication    : CubeDeck 0.2.0\n");
    printf("\x1b[12;2HNote: hardware revision and memory probes are not\n");
    printf("\x1b[13;2Himplemented in this starter build.\n");
    printf("\x1b[21;2HB: Back to main menu\n");
}

static void draw_controller(void)
{
    PADStatus pad[4];
    PAD_Read(pad);
    draw_header();
    printf("\x1b[5;2HCONTROLLER PORT MONITOR\n");
    printf("\x1b[7;2HPort       Status       Buttons\n");

    for (int i = 0; i < 4; ++i) {
        const char *status = pad[i].err == PAD_ERR_NONE ? "Connected" : "No controller";
        int row = 8 + i * 3;
        printf("\x1b[%d;2HPort %d:   %-13s %04X", row, i + 1, status, pad[i].button);
        printf("\x1b[%d;4HStick X/Y: %4d / %4d    C-stick: %4d / %4d",
               row + 1, pad[i].stickX, pad[i].stickY,
               pad[i].substickX, pad[i].substickY);
    }

    printf("\x1b[21;2HConnect pads to any port to monitor input.");
    printf("\x1b[22;2HB: Back to main menu");
}

static void joybus_callback(s32 channel, u32 result)
{
    (void)channel;
    joybus_callback_result = result;
    joybus_callback_done = 1;
    joybus_pending = 0;
}

static void start_joybus_transfer(u8 command, u32 response_length)
{
    memset(joybus_cmd, 0, sizeof(joybus_cmd));
    memset(joybus_res, 0, sizeof(joybus_res));
    joybus_cmd[0] = command;
    joybus_callback_done = 0;
    joybus_callback_result = 0;
    joybus_pending = 1;
    joybus_stage_frame = frame_count;
    SI_Transfer(1, joybus_cmd, 1, joybus_res, response_length,
                joybus_callback, 50);
}

static void start_joybus_word_send(void)
{
    memset(joybus_cmd, 0, sizeof(joybus_cmd));
    memset(joybus_res, 0, sizeof(joybus_res));
    joybus_cmd[0] = 0x15; /* Joybus receive/write command */
    joybus_cmd[1] = (u8)(joybus_test_word);
    joybus_cmd[2] = (u8)(joybus_test_word >> 8);
    joybus_cmd[3] = (u8)(joybus_test_word >> 16);
    joybus_cmd[4] = (u8)(joybus_test_word >> 24);
    joybus_callback_done = 0;
    joybus_callback_result = 0;
    joybus_pending = 1;
    joybus_stage_frame = frame_count;
    SI_Transfer(1, joybus_cmd, 5, joybus_res, 1, joybus_callback, 50);
}

static void service_joybus_probe(void)
{
    if (joybus_pending &&
        frame_count - joybus_stage_frame > JOYBUS_TIMEOUT_FRAMES) {
        /* Do not start another transfer: SI_Transfer has no cancellation API. */
        fail_probe(CDAP_FAIL_WATCHDOG);
        return;
    }

    if (joybus_stage == 11) {
        if ((u32)(frame_count - v2_resume_frame) >= CDAP_STARTUP_GAP_VSYNCS) {
            joybus_stage = 2;
            start_joybus_transfer(0x00, 3);
        }
        return;
    }
    if (joybus_stage == 12) {
        if ((u32)(frame_count - v2_resume_frame) >= CDAP_STARTUP_GAP_VSYNCS) {
            joybus_stage = 3;
            start_joybus_word_send();
        }
        return;
    }
    if (joybus_stage == 8) {
        /* The GBA's CRC verifier runs outside its serial IRQ. Let it
         * finish before asking for the last frame's final verdict. */
        if (frame_count - v2_resume_frame >= V2_FRAME_GAP_VSYNCS) {
            joybus_test_word = CDAP_QUERY(v2_frame_index + 1u);
            joybus_stage = 9;
            start_joybus_word_send();
        }
        return;
    }
    if (!joybus_callback_done)
        return;

    joybus_callback_done = 0;
    if (joybus_callback_result != 0) {
        fail_probe(CDAP_FAIL_SI_CALLBACK);
        return;
    }

    if (joybus_stage == 9) {
        joybus_stage = 10;
        start_joybus_transfer(0x14, 5);
    } else if (joybus_stage == 10) {
        joybus_reply = (u32)joybus_res[0] |
                       ((u32)joybus_res[1] << 8) |
                       ((u32)joybus_res[2] << 16) |
                       ((u32)joybus_res[3] << 24);
        if (joybus_reply == CDAP_ACK(v2_frame_index + 1u)) {
            ack_retries = 0u;
            if (v2_frame_index + 1u == V2_FRAME_COUNT) {
                joybus_stage = 5;
            } else {
                ++v2_frame_index;
                if (build_v2_frame(v2_frame_index) != 0) {
                    fail_probe(CDAP_FAIL_BUILD);
                    return;
                }
                joybus_test_word = joybus_packet_words[0];
                joybus_stage = 3;
                start_joybus_word_send();
            }
        } else if (joybus_reply == CDAP_NACK(v2_frame_index + 1u) ||
                   joybus_reply == CDAP_PENDING) {
            if (++ack_retries > CDAP_MAX_RETRIES) {
                fail_probe(CDAP_FAIL_RETRY_LIMIT);
                return;
            }
            ++ack_recoveries;
            if (build_v2_frame(v2_frame_index) != 0) {
                fail_probe(CDAP_FAIL_BUILD);
                return;
            }
            joybus_test_word = joybus_packet_words[0];
            joybus_stage = 3;
            start_joybus_word_send();
        } else {
            joybus_stage = 6;
        }
    } else if (joybus_stage == 1) {
        /* RESET completed. Continue with the proven STATUS handshake
         * before any framed words or packet-verdict queries. */
        joybus_stage = 11;
        v2_resume_frame = frame_count;
    } else if (joybus_stage == 2) {
        memcpy(joybus_status, joybus_res, sizeof(joybus_status));
        joybus_stage = 3;
        if (joybus_test_mode == 3) {
            joybus_packet_word_index = 0;
            joybus_test_word = joybus_packet_words[0];
        }
        joybus_stage = 12;
        v2_resume_frame = frame_count;
    } else if (joybus_stage == 3) {
        joybus_stage = 4;
        start_joybus_transfer(0x14, 5); /* Read the GBA's 32-bit reply */
    } else if (joybus_stage == 4) {
        joybus_reply = (u32)joybus_res[0] |
                       ((u32)joybus_res[1] << 8) |
                       ((u32)joybus_res[2] << 16) |
                       ((u32)joybus_res[3] << 24);
        if (joybus_reply != (joybus_test_word ^ JOYBUS_REPLY_XOR)) {
            joybus_stage = 6;
        } else if (joybus_test_mode == 3) {
            ++joybus_packet_word_index;
            if (joybus_packet_word_index < joybus_packet_word_count) {
                joybus_test_word = joybus_packet_words[joybus_packet_word_index];
                joybus_stage = 3;
                start_joybus_word_send();
            } else {
                v2_resume_frame = frame_count;
                joybus_stage = 8; /* query GBA's verified frame verdict */
            }
        } else {
            ++joybus_round;
            if (joybus_round < joybus_rounds_target) {
                joybus_test_word = JOYBUS_TEST_WORD_BASE + (u32)joybus_round;
                joybus_stage = 3;
                start_joybus_word_send();
            } else {
                joybus_stage = 5;
            }
        }
    }
}

static void draw_link(void)
{
    draw_header();
    printf("\x1b[5;2HGBA LINK CABLE CHECK\n");
    printf("\x1b[7;2HTarget: GameCube controller Port 2 (SI channel 1)\n");

    switch (link_status) {
        case 1:
            printf("\x1b[9;2HStatus: Checking port...\n");
            break;
        case 2:
            printf("\x1b[9;2HStatus: GBA device detected!\n");
            break;
        case 3:
            printf("\x1b[9;2HStatus: No GBA detected on the last check.\n");
            break;
        default:
            printf("\x1b[9;2HStatus: Waiting for first port check...\n");
            break;
    }

    printf("\x1b[11;2HReported SI device type: 0x%08lX\n", (unsigned long)link_type_value);
    switch (joybus_stage) {
        case 0:
            if (link_status == 2)
                printf("\x1b[13;2HA: framed packet  |  X: one round  |  Y: 16 rounds\n");
            else
                printf("\x1b[13;2HX test unlocks after GBA detection succeeds.\n");
            break;
        case 1:
            printf("\x1b[13;2HJoybus test: waiting for RESET...\n");
            break;
        case 2:
            printf("\x1b[13;2HJoybus test: waiting for STATUS...\n");
            break;
        case 3:
            if (joybus_test_mode == 3)
                printf("\x1b[13;2HFrame %u/3 word %lu/%lu...\n",
                       v2_frame_index + 1u,
                       (unsigned long)(joybus_packet_word_index + 1u),
                       (unsigned long)joybus_packet_word_count);
            else
                printf("\x1b[13;2HSending word %08lX (%d/%d)...\n",
                       (unsigned long)joybus_test_word, joybus_round + 1,
                       joybus_rounds_target);
            break;
        case 4:
            printf("\x1b[13;2HReading GBA diagnostic reply...\n");
            break;
        case 5:
            if (joybus_test_mode == 3) {
                printf("\x1b[13;2HV2 ACK PASS: %u/3 verified\n",
                       V2_FRAME_COUNT);
                printf("\x1b[14;2HRetries: %u   Last ACK: %08lX\n",
                       ack_recoveries,
                       (unsigned long)joybus_reply);
            } else {
                printf("\x1b[13;2HJOYBUS TEST PASSED: %d/%d replies valid\n",
                       joybus_round, joybus_rounds_target);
                printf("\x1b[14;2HLast reply: %08lX  STATUS: %02X %02X %02X\n",
                       (unsigned long)joybus_reply, joybus_status[0],
                       joybus_status[1], joybus_status[2]);
            }
            break;
        case 6:
            if (joybus_test_mode == 3)
                printf("\x1b[13;2HPacket word %lu reply mismatch\n",
                       (unsigned long)(joybus_packet_word_index + 1u));
            else
                printf("\x1b[13;2HReply mismatch on round %d\n", joybus_round + 1);
            printf("\x1b[14;2HGBA: %08lX  Expected: %08lX\n",
                   (unsigned long)joybus_reply,
                   (unsigned long)(joybus_test_word ^ JOYBUS_REPLY_XOR));
            printf("\x1b[15;2HRe-enter page to retry.\n");
            break;
        case 11:
            printf("\x1b[13;2HRESET acknowledged; waiting to query STATUS...\n");
            break;
        case 12:
            printf("\x1b[13;2HSTATUS acknowledged; settling before data...\n");
            break;
        case 9:
        case 10:
            printf("\x1b[13;2HWaiting for GBA packet verdict %u/3...\n",v2_frame_index+1u);
            break;
        case 8:
            printf("\x1b[13;2HFrame %u/3 acknowledged; GBA verifying...\n",
                   v2_frame_index);
            break;
        case 7:
            printf("\x1b[13;2HFAIL code %u at stage %d, frame %u/3\n",
                   failure_reason, failure_stage, v2_frame_index + 1u);
            printf("\x1b[14;2HWord %lu/%lu  retry %u  pending %d\n",
                   (unsigned long)(joybus_packet_word_index + 1u),
                   (unsigned long)joybus_packet_word_count,
                   ack_retries, joybus_pending);
            printf("\x1b[15;2HSI result %08lX  last reply %08lX\n",
                   (unsigned long)failure_callback_result,
                   (unsigned long)failure_reply);
            break;
    }
    printf("\x1b[16;2H3 verified frames, retry test; no ROM/save I/O.\n");
    printf("\x1b[17;2HConnect GBA to Port 2; detection checks repeat every second.\n");
    printf("\x1b[21;2HB: Back to main menu\n");
}

static void draw_about(void)
{
    draw_header();
    printf("\x1b[5;2HABOUT CUBEDECK\n");
    printf("\x1b[7;2HCubeDeck is an open homebrew utility shell.\n");
    printf("\x1b[9;2HTechnology: C, devkitPPC, libogc\n");
    printf("\x1b[10;2HTarget: Nintendo GameCube (DOL executable)\n");
    printf("\x1b[12;2HStandalone EXPERIMENT: V2 framed Joybus word echo.\n");
    printf("\x1b[13;2HTests matched transfer ID and GBA CRC separately.\n");
    printf("\x1b[21;2HB: Back to main menu\n");
}

static void draw_exit(void)
{
    draw_header();
    printf("\x1b[5;2HEXIT CUBEDECK\n");
    printf("\x1b[7;2HTo avoid a crash, CubeDeck does not attempt\n");
    printf("\x1b[8;2Hto return directly to the system menu.\n");
    printf("\x1b[10;2HUse Swiss to launch another app or reset\n");
    printf("\x1b[11;2Hthe console using your normal setup.\n");
    printf("\x1b[21;2HB: Back to main menu\n");
}

static void render(void)
{
    printf("\x1b[2J");
    switch (screen) {
        case SCREEN_HOME:       draw_home(); break;
        case SCREEN_SYSTEM:     draw_system(); break;
        case SCREEN_CONTROLLER: draw_controller(); break;
        case SCREEN_LINK:       draw_link(); break;
        case SCREEN_ABOUT:      draw_about(); break;
        case SCREEN_EXIT:       draw_exit(); break;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    VIDEO_Init();
    PAD_Init();

    video_mode = VIDEO_GetPreferredMode(NULL);
    if (video_mode == NULL) {
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        return 0;
    }

    void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(video_mode));
    if (xfb == NULL) {
        SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
        return 0;
    }

    /* Keep the known-working framebuffer stride unchanged. */
    console_init(xfb, 20, 20, video_mode->fbWidth,
                 video_mode->xfbHeight, video_mode->fbWidth * 2);
    VIDEO_Configure(video_mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (video_mode->viTVMode & VI_NON_INTERLACE)
        VIDEO_WaitVSync();

    while (1) {
        PAD_ScanPads();
        u16 down = PAD_ButtonsDown(0);

        if (screen == SCREEN_HOME) {
            if (down & PAD_BUTTON_UP)
                selected = (selected + MENU_COUNT - 1) % MENU_COUNT;
            if (down & PAD_BUTTON_DOWN)
                selected = (selected + 1) % MENU_COUNT;
            if (down & PAD_BUTTON_A) {
                switch (selected) {
                    case 0: screen = SCREEN_SYSTEM; break;
                    case 1: screen = SCREEN_CONTROLLER; break;
                    case 2:
                        link_status = 0;
                        link_type_value = 0;
                        link_last_query_frame = 0;
                        if (!joybus_pending) {
                            joybus_stage = 0;
                            joybus_status[0] = joybus_status[1] = joybus_status[2] = 0;
                        }
                        screen = SCREEN_LINK;
                        break;
                    case 3: screen = SCREEN_ABOUT; break;
                    case 4: screen = SCREEN_EXIT; break;
                }
            }
        } else if (down & PAD_BUTTON_B) {
            screen = SCREEN_HOME;
        }

        if (screen == SCREEN_LINK &&
            (down & (PAD_BUTTON_A | PAD_BUTTON_X | PAD_BUTTON_Y)) &&
            link_status == 2 && !link_query_pending && !joybus_pending &&
            joybus_stage == 0) {
            joybus_round = 0;
            joybus_rounds_target = (down & PAD_BUTTON_Y) ? JOYBUS_STRESS_ROUNDS : 1;
            joybus_test_word = JOYBUS_TEST_WORD_BASE;
            if (down & PAD_BUTTON_A) {
                v2_frame_index = 0u;
                failure_reason = CDAP_FAIL_NONE;
                failure_stage = 0;
                failure_callback_result = 0u;
                failure_reply = 0u;
                ack_retries = 0u;
                ack_recoveries = 0u;
                if (build_v2_frame(0u) != 0) {
                    joybus_stage = 7;
                    joybus_test_mode = 3;
                    continue;
                }
                joybus_test_mode = 3;
            } else {
                joybus_test_mode = (down & PAD_BUTTON_Y) ? 2 : 1;
            }
            joybus_stage = 1;
            start_joybus_transfer(0xFF, 3); /* Joybus RESET */
        }

        if (screen == SCREEN_LINK && ((joybus_stage >= 1 && joybus_stage <= 4) ||
                (joybus_stage >= 8 && joybus_stage <= 12)))
            service_joybus_probe();

        if (screen == SCREEN_LINK && joybus_stage == 0 && !link_query_pending &&
            !joybus_pending &&
            (link_last_query_frame == 0 ||
             frame_count - link_last_query_frame >= LINK_QUERY_INTERVAL)) {
            link_query_pending = 1;
            link_status = 1;
            link_last_query_frame = frame_count ? frame_count : 1;
            SI_GetTypeAsync(1, link_type_callback);
        }

        render();
        VIDEO_WaitVSync();
        ++frame_count;
    }

    return 0;
}
