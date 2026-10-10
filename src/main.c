#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MENU_COUNT 4

typedef enum {
    SCREEN_HOME = 0,
    SCREEN_SYSTEM,
    SCREEN_CONTROLLER,
    SCREEN_ABOUT,
    SCREEN_EXIT
} Screen;

static GXRModeObj *video_mode;
static u32 frame_count;
static int selected = 0;
static Screen screen = SCREEN_HOME;
static const char *menu_items[MENU_COUNT] = {
    "System information",
    "Controller test",
    "About CubeDeck",
    "Exit (use Swiss reset)"
};

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

    printf("\x1b[20;2HCONTROLS\n");
    printf("\x1b[21;2HD-Pad: Move    A: Select    B: Back\n");
    printf("\x1b[23;2HBuild 0.1.0  |  GameCube / libogc\n");
}

static void draw_system(void)
{
    draw_header();
    printf("\x1b[5;2HSYSTEM INFORMATION\n");
    printf("\x1b[7;2HConsole target : Nintendo GameCube\n");
    printf("\x1b[8;2HCPU family     : IBM PowerPC 750 (Gekko)\n");
    printf("\x1b[9;2HVideo mode     : libogc preferred mode\n");
    printf("\x1b[10;2HApplication    : CubeDeck 0.1.0\n");
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

static void draw_about(void)
{
    draw_header();
    printf("\x1b[5;2HABOUT CUBEDECK\n");
    printf("\x1b[7;2HCubeDeck is an open homebrew utility shell.\n");
    printf("\x1b[9;2HTechnology: C, devkitPPC, libogc\n");
    printf("\x1b[10;2HTarget: Nintendo GameCube (DOL executable)\n");
    printf("\x1b[12;2HThis build is a foundation, not a finished\n");
    printf("\x1b[13;2Hsystem manager. Features will be added in tested\n");
    printf("\x1b[14;2Hsteps after a successful hardware boot.\n");
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
                    case 2: screen = SCREEN_ABOUT; break;
                    case 3: screen = SCREEN_EXIT; break;
                }
            }
        } else if (down & PAD_BUTTON_B) {
            screen = SCREEN_HOME;
        }

        render();
        VIDEO_WaitVSync();
        ++frame_count;
    }

    return 0;
}
