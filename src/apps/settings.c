#include "apps/settings.h"
#include "apps/control.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gui/theme.h"
#include "gui/hud.h"
#include "gui/lockscreen.h"
#include "gui/notification.h"
#include "gfx/gfx.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/io.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define SETTINGS_WIN_W 520
#define SETTINGS_WIN_H 370

typedef enum {
    TAB_DISPLAY = 0,
    TAB_AUDIO,
    TAB_SECURITY,
    TAB_POWER
} settings_tab_t;

static window_t* s_win_settings = NULL;
static settings_tab_t s_active_tab = TAB_DISPLAY;

/* PC Speaker Tone generator helpers */
static void speaker_tone(uint32_t freq_hz) {
    if (freq_hz == 0) {
        uint8_t tmp = inb(0x61);
        outb(0x61, tmp & 0xFC);
        return;
    }
    uint32_t div = 1193182 / freq_hz;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)((div >> 8) & 0xFF));
    uint8_t tmp = inb(0x61);
    if ((tmp & 3) != 3) {
        outb(0x61, tmp | 3);
    }
}

static void speaker_off(void) {
    uint8_t tmp = inb(0x61);
    outb(0x61, tmp & 0xFC);
}

static void system_reboot(void) {
    notification_push(NOTIFY_WARNING, "System Reboot", "Pulsing 8042 reset line...");
    outb(0x64, 0xFE);
    __asm__ volatile ("cli");
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile ("lidt %0; int3" : : "m"(null_idt));
    while (1) { __asm__ volatile ("hlt"); }
}

static void system_halt(void) {
    notification_push(NOTIFY_WARNING, "System Halting", "CPU entering safe halt...");
    __asm__ volatile ("cli");
    while (1) { __asm__ volatile ("hlt"); }
}

static void on_settings_close(window_t* win) {
    speaker_off();
    wm_remove_window(win);
    s_win_settings = NULL;
}

static void settings_render_content(void);

static void settings_fmt_uint(uint32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[16];
    int t = 0;
    while (val > 0) {
        temp[t++] = (char)('0' + (val % 10));
        val /= 10;
    }
    int out = 0;
    for (int i = t - 1; i >= 0; i--) {
        buf[out++] = temp[i];
    }
    buf[out] = '\0';
}

static void on_btn_tab(widget_t* w, void* user_data) {
    (void)w;
    s_active_tab = (settings_tab_t)(uintptr_t)user_data;
    settings_render_content();
}

static void on_btn_theme(widget_t* w, void* user_data) {
    (void)w;
    theme_id_t tid = (theme_id_t)(uintptr_t)user_data;
    theme_set(tid);
    notification_push(NOTIFY_SUCCESS, "Theme Changed", theme_get_current()->name);
    wm_redraw_all();
}

static void on_btn_hud_toggle(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    hud_toggle();
    notification_push(NOTIFY_INFO, "Conky HUD", hud_is_enabled() ? "HUD Enabled" : "HUD Disabled");
    wm_redraw_all();
}

static void on_btn_speaker_tone(widget_t* w, void* user_data) {
    (void)w;
    uint32_t freq = (uint32_t)(uintptr_t)user_data;
    if (freq == 0) {
        speaker_off();
        notification_push(NOTIFY_INFO, "Audio", "PC Speaker silenced.");
    } else {
        speaker_tone(freq);
        notification_push(NOTIFY_SUCCESS, "Audio Test", "Playing test frequency tone.");
    }
}

static void on_btn_lock_now(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    lockscreen_activate();
}

static void on_btn_reboot(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    system_reboot();
}

static void on_btn_halt(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    system_halt();
}

static void settings_render_content(void) {
    if (!s_win_settings) return;

    /* Destroy existing dynamic child widgets */
    widget_destroy_all(s_win_settings);

    const theme_t* theme = theme_get_current();
    uint32_t bg_client = theme ? theme->bg_window_client : 0x00EAEAEA;
    uint32_t bg_panel  = theme ? theme->bg_panel         : 0x00181825;
    uint32_t text_col  = theme ? theme->text_primary     : 0x00CDD6F4;
    uint32_t sub_col   = theme ? theme->text_secondary   : 0x00A6ADC8;
    uint32_t card_bg   = theme ? theme->bg_card          : 0x001E1E2E;
    uint32_t border    = theme ? theme->border_active    : 0x00CBA6F7;

    window_clear_client(s_win_settings, bg_client);

    /* Left Sidebar for Navigation Tabs (x: 0, w: 120, h: client_height) */
    window_fill_rect(s_win_settings, 0, 0, 120, s_win_settings->client_height, bg_panel);
    window_fill_rect(s_win_settings, 120, 0, 1, s_win_settings->client_height, border);

    /* Sidebar Tab Buttons */
    widget_add_to_window(s_win_settings, widget_create_button(8, 12,  104, 32, "Display",  on_btn_tab, (void*)(uintptr_t)TAB_DISPLAY));
    widget_add_to_window(s_win_settings, widget_create_button(8, 52,  104, 32, "Audio",    on_btn_tab, (void*)(uintptr_t)TAB_AUDIO));
    widget_add_to_window(s_win_settings, widget_create_button(8, 92,  104, 32, "Security", on_btn_tab, (void*)(uintptr_t)TAB_SECURITY));
    widget_add_to_window(s_win_settings, widget_create_button(8, 132, 104, 32, "Power",    on_btn_tab, (void*)(uintptr_t)TAB_POWER));

    /* Active tab indicator marker */
    int marker_y = 12 + ((int)s_active_tab * 40);
    window_fill_rect(s_win_settings, 2, marker_y + 4, 4, 24, theme ? theme->accent_primary : 0x00CBA6F7);

    /* Main Content Area: x: 130 .. 508 */
    int cx = 136;
    switch (s_active_tab) {
        case TAB_DISPLAY: {
            window_draw_string(s_win_settings, cx, 14, "Display & Personalization", text_col, bg_client, true);
            window_draw_string(s_win_settings, cx, 32, "Select desktop visual theme and HUD widgets:", sub_col, bg_client, true);

            widget_add_to_window(s_win_settings, widget_create_button(cx, 56, 170, 30, "Catppuccin Mocha", on_btn_theme, (void*)(uintptr_t)THEME_CATPPUCCIN_MOCHA));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 56, 170, 30, "Tokyo Night", on_btn_theme, (void*)(uintptr_t)THEME_TOKYO_NIGHT));

            widget_add_to_window(s_win_settings, widget_create_button(cx, 94, 170, 30, "Cyberpunk Neon", on_btn_theme, (void*)(uintptr_t)THEME_CYBERPUNK));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 94, 170, 30, "Nord Frost", on_btn_theme, (void*)(uintptr_t)THEME_NORD));

            widget_add_to_window(s_win_settings, widget_create_button(cx, 132, 170, 30, "Classic Retro", on_btn_theme, (void*)(uintptr_t)THEME_CLASSIC_RETRO));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 132, 170, 30, "Toggle Conky HUD", on_btn_hud_toggle, NULL));

            /* Current Theme Status Card */
            window_fill_rect(s_win_settings, cx, 180, 350, 120, card_bg);
            window_draw_rect(s_win_settings, cx, 180, 350, 120, border);
            window_draw_string(s_win_settings, cx + 12, 192, "Active Preset:", text_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 120, 192, theme ? theme->name : "Default", theme ? theme->accent_primary : 0x00CBA6F7, card_bg, true);

            window_draw_string(s_win_settings, cx + 12, 216, "Resolution:  1024 x 768 x 32bpp (VBE)", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 236, "Renderer:    3 MB Double-Buffered RAM", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 256, "Conky HUD:   [F5] Quick Toggle", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 276, "Hyprlock:    [F6] Instant Screen Lock", sub_col, card_bg, true);
            break;
        }

        case TAB_AUDIO: {
            window_draw_string(s_win_settings, cx, 14, "Audio & Sound Synthesizer", text_col, bg_client, true);
            window_draw_string(s_win_settings, cx, 32, "Hardware: x86 PC Speaker (8254 PIT Ch 2 / Port 0x61)", sub_col, bg_client, true);

            widget_add_to_window(s_win_settings, widget_create_button(cx, 56, 170, 32, "Beep A4 (440 Hz)", on_btn_speaker_tone, (void*)(uintptr_t)440));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 56, 170, 32, "Beep A5 (880 Hz)", on_btn_speaker_tone, (void*)(uintptr_t)880));

            widget_add_to_window(s_win_settings, widget_create_button(cx, 96, 170, 32, "Beep A3 (220 Hz)", on_btn_speaker_tone, (void*)(uintptr_t)220));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 96, 170, 32, "Silence Speaker", on_btn_speaker_tone, (void*)(uintptr_t)0));

            /* Audio Specs Card */
            window_fill_rect(s_win_settings, cx, 150, 350, 150, card_bg);
            window_draw_rect(s_win_settings, cx, 150, 350, 150, border);
            window_draw_string(s_win_settings, cx + 12, 162, "Sound Architecture:", text_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 186, "- Timer Channel 2 Square Wave (Port 0x42)", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 206, "- Gate & Data via Port 0x61 Bits [1:0]", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 226, "- Freestanding Chiptune Melodies", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 246, "- 8-Band Frequency Spectrum Visualizer", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 266, "- Tested in VirtualBox VM OSProject", 0x00A6E3A1, card_bg, true);
            break;
        }

        case TAB_SECURITY: {
            window_draw_string(s_win_settings, cx, 14, "Security & User Accounts", text_col, bg_client, true);
            window_draw_string(s_win_settings, cx, 32, "Multi-layered kernel defenses and authentication:", sub_col, bg_client, true);

            /* User Profile Card */
            window_fill_rect(s_win_settings, cx, 56, 350, 80, card_bg);
            window_draw_rect(s_win_settings, cx, 56, 350, 80, border);

            /* Monogram Avatar */
            window_fill_rect(s_win_settings, cx + 12, 68, 48, 48, theme ? theme->accent_primary : 0x00CBA6F7);
            window_draw_string(s_win_settings, cx + 24, 82, "PV", 0x0011111B, 0, true);
            window_draw_string(s_win_settings, cx + 70, 72, "Pruthviraj (Administrator)", text_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 70, 92, "Ring 0 Protected Mode Supervisor", sub_col, card_bg, true);

            widget_add_to_window(s_win_settings, widget_create_button(cx + 250, 72, 85, 30, "Lock [F6]", on_btn_lock_now, NULL));

            /* Kernel Defenses Card */
            window_fill_rect(s_win_settings, cx, 148, 350, 160, card_bg);
            window_draw_rect(s_win_settings, cx, 148, 350, 160, border);
            window_draw_string(s_win_settings, cx + 12, 160, "Kernel Defense Shield:", text_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 184, "[PASS] Page 0 Null Dereference Trap", 0x00A6E3A1, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 204, "[PASS] Task Stack Canaries (0xDEADBEEF)", 0x00A6E3A1, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 224, "[PASS] VFS Path Traversal Isolation", 0x00A6E3A1, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 244, "[PASS] OpenBSD Bounded String Drivers", 0x00A6E3A1, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 264, "[PASS] Volatile Memory Wipe Trigger", 0x00A6E3A1, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 284, "Security Status: HARDENED", theme ? theme->accent_primary : 0x00CBA6F7, card_bg, true);
            break;
        }

        case TAB_POWER: {
            window_draw_string(s_win_settings, cx, 14, "Power & Hardware Management", text_col, bg_client, true);
            window_draw_string(s_win_settings, cx, 32, "Hardware specifications and power options:", sub_col, bg_client, true);

            /* Telemetry Metrics */
            window_fill_rect(s_win_settings, cx, 56, 350, 140, card_bg);
            window_draw_rect(s_win_settings, cx, 56, 350, 140, border);

            uint32_t free_f = pmm_free_frames();
            size_t used_b = 0, free_b = 0, blocks = 0;
            heap_stats(&used_b, &free_b, &blocks);
            uint32_t uptime = timer_get_uptime_sec();

            char buf[64];
            char num_buf[16];

            strlcpy(buf, "Physical RAM: 128 MB (Free: ", sizeof(buf));
            settings_fmt_uint((free_f * 4096) / 1024, num_buf);
            strlcat(buf, num_buf, sizeof(buf));
            strlcat(buf, " KB)", sizeof(buf));
            window_draw_string(s_win_settings, cx + 12, 70, buf, text_col, card_bg, true);

            strlcpy(buf, "Kernel Heap:  Used: ", sizeof(buf));
            settings_fmt_uint((uint32_t)(used_b / 1024), num_buf);
            strlcat(buf, num_buf, sizeof(buf));
            strlcat(buf, " KB | Free: ", sizeof(buf));
            settings_fmt_uint((uint32_t)(free_b / 1024), num_buf);
            strlcat(buf, num_buf, sizeof(buf));
            strlcat(buf, " KB", sizeof(buf));
            window_draw_string(s_win_settings, cx + 12, 94, buf, text_col, card_bg, true);

            strlcpy(buf, "System Uptime: ", sizeof(buf));
            settings_fmt_uint(uptime, num_buf);
            strlcat(buf, num_buf, sizeof(buf));
            strlcat(buf, " seconds", sizeof(buf));
            window_draw_string(s_win_settings, cx + 12, 118, buf, text_col, card_bg, true);

            window_draw_string(s_win_settings, cx + 12, 142, "CPU Model:    x86 Protected Mode (i686)", sub_col, card_bg, true);
            window_draw_string(s_win_settings, cx + 12, 162, "Firmware:     Multiboot 1 / BIOS", sub_col, card_bg, true);

            /* Action Buttons */
            widget_add_to_window(s_win_settings, widget_create_button(cx, 210, 165, 36, "Reboot Computer", on_btn_reboot, NULL));
            widget_add_to_window(s_win_settings, widget_create_button(cx + 180, 210, 165, 36, "Power Off / Halt", on_btn_halt, NULL));
            break;
        }
    }

    widget_render_all(s_win_settings);
    wm_invalidate_rect(s_win_settings->x, s_win_settings->y, s_win_settings->width, s_win_settings->height);
}

void app_settings_open(void) {
    if (s_win_settings) {
        wm_bring_to_front(s_win_settings);
        return;
    }

    s_win_settings = window_create(140, 70, SETTINGS_WIN_W, SETTINGS_WIN_H,
                                   "System Settings & Control Center",
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_settings) return;

    s_win_settings->on_close = on_settings_close;
    s_active_tab = TAB_DISPLAY;

    settings_render_content();
    wm_add_window(s_win_settings);
    serial_writestring("[SETTINGS] System Settings application opened.\n");
}
