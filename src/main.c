#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_COUNT 5
#define LINK_QUERY_INTERVAL 60

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
static int joybus_stage = 0; /* 0=idle, 1=RESET, 2=STATUS, 3=done, 4=failed */
static u32 joybus_stage_frame = 0;
static u8 joybus_status[3] = {0, 0, 0};
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
    printf("\x1b[2;2HCUBEDECK  |  GAMECUBE HOMEBREW\n");
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
    printf("\x1b[23;2HBuild 0.2.0  |  GameCube / libogc\n");
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

static void service_joybus_probe(void)
{
    if (joybus_pending &&
        frame_count - joybus_stage_frame > JOYBUS_TIMEOUT_FRAMES) {
        /* Do not start another transfer: SI_Transfer has no cancellation API. */
        joybus_stage = 4;
        return;
    }

    if (!joybus_callback_done)
        return;

    joybus_callback_done = 0;
    if (joybus_callback_result != 0) {
        joybus_stage = 4;
        return;
    }

    if (joybus_stage == 1) {
        joybus_stage = 2;
        start_joybus_transfer(0x00, 3); /* Joybus STATUS */
    } else if (joybus_stage == 2) {
        memcpy(joybus_status, joybus_res, sizeof(joybus_status));
        joybus_stage = 3;
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
    printf("\x1b[13;2HThis is detection only: no Joybus commands are sent,\n");
    printf("\x1b[14;2Hand no cartridge or save data is accessed.\n");
    printf("\x1b[16;2HConnect the official/compatible GBA link cable to\n");
    printf("\x1b[17;2HPort 2 and power on the GBA. Checks repeat every second.\n");
    printf("\x1b[21;2HB: Back to main menu\n");
}

static void draw_about(void)
{
    draw_header();
    printf("\x1b[5;2HABOUT CUBEDECK\n");
    printf("\x1b[7;2HCubeDeck is an open homebrew utility shell.\n");
    printf("\x1b[9;2HTechnology: C, devkitPPC, libogc\n");
    printf("\x1b[10;2HTarget: Nintendo GameCube (DOL executable)\n");
    printf("\x1b[12;2HThe GBA link page currently checks device presence;\n");
    printf("\x1b[13;2Hit does not yet transfer data or launch a GBA payload.\n");
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

        if (screen == SCREEN_LINK && (down & PAD_BUTTON_X) &&
            link_status == 2 && !link_query_pending && !joybus_pending &&
            joybus_stage == 0) {
            joybus_stage = 1;
            start_joybus_transfer(0xFF, 3); /* Joybus RESET */
        }

        if (screen == SCREEN_LINK && (joybus_stage == 1 || joybus_stage == 2))
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
