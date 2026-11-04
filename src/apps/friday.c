/*
 * friday.c – Friday Multilingual AI Assistant Subsystem for Friday OS
 *
 * Implements the kernel-side assistant that listens for command packets
 * from the host Python daemon over COM2 (0x2F8, IRQ 3), executes OS actions,
 * produces audio chimes via PC speaker, shows bilingual toast notifications,
 * and renders an animated AI orb widget on the desktop back buffer.
 */

#include "apps/friday.h"
#include "drivers/serial2.h"   /* COM2 interrupt-driven serial driver */
#include "drivers/serial.h"    /* COM1 kernel debug logging            */
#include "gui/notification.h"  /* Toast notification system           */
#include "gui/theme.h"         /* Theme engine presets                */
#include "gui/wm.h"            /* Window manager recomposite          */
#include "gui/lockscreen.h"    /* Hyprlock screen activation          */
#include "apps/player.h"       /* speaker_play_tone(), speaker_stop() */
#include "mm/pmm.h"            /* pmm_free_frames(), pmm_total_frames()*/
#include "lib/string.h"        /* strcmp, strlen, strlcpy, strlcat    */
#include "lib/kprintf.h"       /* Kernel formatting / logging         */
#include "gfx/gfx.h"           /* 2D drawing primitives               */
#include "gfx/font.h"          /* Font metrics and text rendering     */

/* ------------------------------------------------------------------ */
/*  Weakly linked application entry points                            */
/*  Allows compiling even if individual apps are disabled or modified  */
/* ------------------------------------------------------------------ */

void app_calc_open(void)        __attribute__((weak));
void app_terminal_open(void)    __attribute__((weak));
void app_explorer_open(void)    __attribute__((weak));
void app_editor_open(void)      __attribute__((weak));
void app_sheet_open(void)       __attribute__((weak));
void app_photos_open(void)      __attribute__((weak));
void app_player_open(void)      __attribute__((weak));
void app_breakout_open(void)    __attribute__((weak));
void app_minesweeper_open(void) __attribute__((weak));
void app_snake_open(void)       __attribute__((weak));
void app_settings_open(void)    __attribute__((weak));
void app_taskmgr_open(void)     __attribute__((weak));
void app_clock_open(void)       __attribute__((weak));
void app_store_open(void)       __attribute__((weak));

/* ------------------------------------------------------------------ */
/*  Internal State Variables                                          */
/* ------------------------------------------------------------------ */

static bool           s_online       = false;
static friday_state_t s_state        = FRIDAY_IDLE;
static uint32_t       s_exec_ticks   = 0;  /* Countdown for EXECUTING visual state */
static uint32_t       s_orb_phase    = 0;  /* Pulsing animation phase counter      */
static bool           s_orb_visible  = true;

/* PC Speaker Chime State Machine */
static uint32_t       s_chime_ticks  = 0;
static uint8_t        s_chime_phase  = 0;  /* 0=off, 1=tone1, 2=tone2 */

/* ------------------------------------------------------------------ */
/*  Audio Chime Helpers                                               */
/* ------------------------------------------------------------------ */

/*
 * friday_chime_ok:
 * Plays an upbeat two-tone sci-fi confirmation chime (880 Hz -> 1320 Hz).
 */
static void friday_chime_ok(void) {
    speaker_play_tone(880);   /* A5 note */
    s_chime_phase = 1;
    s_chime_ticks = 4;        /* ~40 ms at 100 Hz timer tick */
}

/*
 * friday_chime_error:
 * Plays a low error beep (220 Hz).
 */
static void friday_chime_error(void) {
    speaker_play_tone(220);   /* A3 note */
    s_chime_phase = 10;       /* Special single-tone phase */
    s_chime_ticks = 8;        /* ~80 ms duration */
}

/* ------------------------------------------------------------------ */
/*  Helper: Format integer without libc                               */
/* ------------------------------------------------------------------ */

static void friday_fmt_uint(uint32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int t = 0;
    while (val > 0) {
        tmp[t++] = (char)('0' + (val % 10));
        val /= 10;
    }
    int b = 0;
    for (int i = t - 1; i >= 0; i--) {
        buf[b++] = tmp[i];
    }
    buf[b] = '\0';
}

/* ------------------------------------------------------------------ */
/*  Self Destruct Protocol Routine                                    */
/* ------------------------------------------------------------------ */

static void friday_self_destruct(void) {
    serial_writestring("\n[FRIDAY] !!! EMERGENCY SELF-DESTRUCT TRIGGERED VIA VOICE/REMOTE !!!\n");
    notification_push(NOTIFY_SECURITY, "Friday Security", "SELF-DESTRUCT INITIATED!");

    /* Audio alert */
    speaker_play_tone(150);

    /* Send response to host */
    serial2_write_string("RESP:DESTRUCT:COMPLETE\n");
}

/* ------------------------------------------------------------------ */
/*  Command Dispatch Table                                            */
/* ------------------------------------------------------------------ */

/*
 * friday_dispatch_command:
 * Parses a single line protocol packet and executes the corresponding OS action.
 */
static void friday_dispatch_command(char* cmd) {
    /* Strip potential trailing CR or whitespace */
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == '\r' || cmd[len - 1] == '\n' || cmd[len - 1] == ' ')) {
        cmd[len - 1] = '\0';
        len--;
    }

    if (len == 0) return;

    serial_writestring("[FRIDAY] Received packet: ");
    serial_writestring(cmd);
    serial_writestring("\n");

    /* 1. App Launchers */
    if (strcmp(cmd, "CMD:LAUNCH:CALC") == 0) {
        if (app_calc_open) app_calc_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Calculator");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:TERM") == 0) {
        if (app_terminal_open) app_terminal_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Terminal");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:EXPLORER") == 0) {
        if (app_explorer_open) app_explorer_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened File Explorer");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:EDITOR") == 0) {
        if (app_editor_open) app_editor_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Text Editor");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:SHEETS") == 0) {
        if (app_sheet_open) app_sheet_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Spreadsheet");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:PHOTOS") == 0) {
        if (app_photos_open) app_photos_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Photos Viewer");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:PLAYER") == 0) {
        if (app_player_open) app_player_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Playing Chiptune Audio");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:BREAKOUT") == 0) {
        if (app_breakout_open) app_breakout_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Launched Breakout Arcade");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:MINES") == 0) {
        if (app_minesweeper_open) app_minesweeper_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Launched Minesweeper");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:SNAKE") == 0) {
        if (app_snake_open) app_snake_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Launched Snake Game");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:SETTINGS") == 0) {
        if (app_settings_open) app_settings_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened System Settings");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:TASKMGR") == 0) {
        if (app_taskmgr_open) app_taskmgr_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Task Manager");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:CLOCK") == 0) {
        if (app_clock_open) app_clock_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Clock & Calendar");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:LAUNCH:STORE") == 0) {
        if (app_store_open) app_store_open();
        notification_push(NOTIFY_INFO, "Friday AI", "Opened Web App Store");
        friday_chime_ok();
    } else if (strncmp(cmd, "CMD:INSTALL:", 12) == 0) {
        const char* pkg = cmd + 12;
        extern bool app_store_install_by_id(const char*);
        if (app_store_install_by_id(pkg)) {
            friday_chime_ok();
            serial2_write_string("RESP:INSTALL:OK\n");
        } else {
            friday_chime_error();
            serial2_write_string("RESP:INSTALL:FAIL\n");
        }
    }
    /* 2. Desktop Theme Presets */
    else if (strcmp(cmd, "CMD:THEME:WINDOWS") == 0) {
        theme_set(THEME_WINDOWS_11);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Windows 11 Fluent");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:THEME:CATPPUCCIN") == 0) {
        theme_set(THEME_CATPPUCCIN_MOCHA);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Catppuccin Mocha");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:THEME:TOKYO") == 0) {
        theme_set(THEME_TOKYO_NIGHT);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Tokyo Night");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:THEME:CYBERPUNK") == 0) {
        theme_set(THEME_CYBERPUNK);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Cyberpunk Neon");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:THEME:NORD") == 0) {
        theme_set(THEME_NORD);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Nord Frost");
        friday_chime_ok();
    } else if (strcmp(cmd, "CMD:THEME:RETRO") == 0) {
        theme_set(THEME_CLASSIC_RETRO);
        wm_redraw_all();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Theme: Classic Retro");
        friday_chime_ok();
    }
    /* 3. Security & Lock Screen */
    else if (strcmp(cmd, "CMD:LOCK") == 0) {
        lockscreen_activate();
        notification_push(NOTIFY_SUCCESS, "Friday AI", "Screen locked");
        friday_chime_ok();
    }
    /* 4. Self Destruct */
    else if (strcmp(cmd, "CMD:SELF_DESTRUCT") == 0) {
        friday_self_destruct();
    }
    /* 5. Telemetry Queries */
    else if (strcmp(cmd, "QUERY:RAM") == 0) {
        uint32_t free_f = pmm_free_frames();
        uint32_t total_f = pmm_total_frames();
        char resp[64] = "RESP:RAM:FREE=";
        char num[16];
        friday_fmt_uint(free_f, num);
        strlcat(resp, num, sizeof(resp));
        strlcat(resp, ":TOTAL=", sizeof(resp));
        friday_fmt_uint(total_f, num);
        strlcat(resp, num, sizeof(resp));
        strlcat(resp, "\n", sizeof(resp));

        serial2_write_string(resp);
        notification_push(NOTIFY_INFO, "Friday AI", "Reported RAM telemetry");
        friday_chime_ok();
    }
    /* 6. Unknown Command */
    else {
        serial_writestring("[FRIDAY] Unknown command: ");
        serial_writestring(cmd);
        serial_writestring("\n");
        notification_push(NOTIFY_WARNING, "Friday AI", "Unknown command");
        friday_chime_error();
    }

    /* Set executing state for visual feedback */
    s_state = FRIDAY_EXECUTING;
    s_exec_ticks = 30; /* ~300 ms highlight */
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

/*
 * friday_init:
 * Sets up COM2 UART with interrupt receive and initialises state.
 */
void friday_init(void) {
    serial2_init();
    s_online = true;
    s_state = FRIDAY_IDLE;
    s_exec_ticks = 0;
    s_orb_phase = 0;
    s_chime_ticks = 0;
    s_chime_phase = 0;
    s_orb_visible = true;

    serial_writestring("[FRIDAY] Friday AI Assistant online. Awaiting commands on COM2 (0x2F8).\n");
    notification_push(NOTIFY_SUCCESS, "Friday AI", "Online & listening on COM2!");
}

/*
 * friday_tick:
 * Periodic tick handler (~100 Hz).
 */
void friday_tick(void) {
    if (!s_online) return;

    /* Advance animation phase */
    s_orb_phase++;

    /* Manage EXECUTING visual timeout */
    if (s_exec_ticks > 0) {
        s_exec_ticks--;
        if (s_exec_ticks == 0) {
            s_state = FRIDAY_IDLE;
        }
    }

    /* Manage PC speaker chime progression */
    if (s_chime_ticks > 0) {
        s_chime_ticks--;
        if (s_chime_ticks == 0) {
            if (s_chime_phase == 1) {
                /* Play tone 2 (1320 Hz) for ~60ms */
                speaker_play_tone(1320);
                s_chime_phase = 2;
                s_chime_ticks = 6;
            } else {
                /* Silence speaker */
                speaker_stop();
                s_chime_phase = 0;
            }
        }
    }

    /* Poll COM2 for any complete line */
    char line_buf[256];
    int len = serial2_read_line(line_buf, sizeof(line_buf));
    if (len > 0) {
        friday_dispatch_command(line_buf);
    }
}

/*
 * friday_is_online:
 */
bool friday_is_online(void) {
    return s_online;
}

/*
 * friday_get_state:
 */
friday_state_t friday_get_state(void) {
    return s_state;
}

/* ------------------------------------------------------------------ */
/*  Bresenham Circle Drawing Helpers for Back Buffer                  */
/* ------------------------------------------------------------------ */

static inline void set_pixel_checked(uint32_t* backbuf, uint32_t fb_width, int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)fb_width && y >= 0 && y < 732) { /* 768 - 36 taskbar */
        backbuf[y * fb_width + x] = color;
    }
}

static void friday_draw_circle(uint32_t* backbuf, uint32_t fb_width, int cx, int cy, int radius, uint32_t color) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;

    while (y >= x) {
        set_pixel_checked(backbuf, fb_width, cx + x, cy + y, color);
        set_pixel_checked(backbuf, fb_width, cx - x, cy + y, color);
        set_pixel_checked(backbuf, fb_width, cx + x, cy - y, color);
        set_pixel_checked(backbuf, fb_width, cx - x, cy - y, color);
        set_pixel_checked(backbuf, fb_width, cx + y, cy + x, color);
        set_pixel_checked(backbuf, fb_width, cx - y, cy + x, color);
        set_pixel_checked(backbuf, fb_width, cx + y, cy - x, color);
        set_pixel_checked(backbuf, fb_width, cx - y, cy - x, color);

        if (d <= 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

static void friday_fill_circle(uint32_t* backbuf, uint32_t fb_width, int cx, int cy, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                set_pixel_checked(backbuf, fb_width, cx + x, cy + y, color);
            }
        }
    }
}

/*
 * friday_render_orb:
 * Renders the pulsing animated orb and state indicator.
 */
void friday_render_orb(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height) {
    if (!s_online || !s_orb_visible || !backbuf) return;
    (void)fb_height;

    int cx = 930;
    int cy = 600;

    uint32_t core_color;
    uint32_t ring_color;

    switch (s_state) {
        case FRIDAY_LISTENING:
            core_color = 0x0094E2D5; /* Cyan */
            ring_color = 0x0089DCFE;
            break;
        case FRIDAY_THINKING:
            core_color = 0x00CBA6F7; /* Violet */
            ring_color = 0x00B4BEFE;
            break;
        case FRIDAY_EXECUTING:
            core_color = 0x00A6E3A1; /* Green */
            ring_color = 0x0094E2D5;
            break;
        default: /* FRIDAY_IDLE */
            core_color = 0x0089B4FA; /* Blue */
            ring_color = 0x0045475A;
            break;
    }

    /* Gentle pulsing calculation */
    int pulse = (int)((s_orb_phase / 4) % 10) - 5;
    if (pulse < 0) pulse = -pulse;

    /* Outermost glow ring */
    friday_draw_circle(backbuf, fb_width, cx, cy, 38 + pulse, ring_color);

    /* Secondary ring */
    friday_draw_circle(backbuf, fb_width, cx, cy, 26 + pulse / 2, core_color);

    /* Solid glowing nucleus */
    friday_fill_circle(backbuf, fb_width, cx, cy, 14, core_color);

    /* Friday label */
    const char* label = "* Friday";
    if (s_state == FRIDAY_EXECUTING) {
        label = "* Active";
    }
    gfx_draw_string(cx - 32, cy + 46, label, core_color, 0x00181825, true);
}
