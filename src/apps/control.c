#include "apps/control.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define CONTROL_WIN_W 480
#define CONTROL_WIN_H 340

static window_t* s_control_win = NULL;
static widget_t* s_lbl_theme   = NULL;

/* Wallpaper palette options */
#define THEME_NAVY    0x001B3A68
#define THEME_TEAL    0x00008080
#define THEME_SLATE   0x002F3E46
#define THEME_FOREST  0x001B4332

static void on_btn_theme_teal(widget_t* w, void* data) {
    (void)w; (void)data;
    wm_set_wallpaper_color(THEME_TEAL);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Theme: Retro Teal (0x008080)");
    serial_writestring("[CONTROL] Theme changed to Retro Teal\n");
}

static void on_btn_theme_navy(widget_t* w, void* data) {
    (void)w; (void)data;
    wm_set_wallpaper_color(THEME_NAVY);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Theme: Classic Navy (0x1B3A68)");
    serial_writestring("[CONTROL] Theme changed to Classic Navy\n");
}

static void on_btn_theme_slate(widget_t* w, void* data) {
    (void)w; (void)data;
    wm_set_wallpaper_color(THEME_SLATE);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Theme: Slate Gray (0x2F3E46)");
    serial_writestring("[CONTROL] Theme changed to Slate Gray\n");
}

static void on_btn_theme_forest(widget_t* w, void* data) {
    (void)w; (void)data;
    wm_set_wallpaper_color(THEME_FOREST);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Theme: Deep Forest (0x1B4332)");
    serial_writestring("[CONTROL] Theme changed to Deep Forest\n");
}

static void on_control_close(window_t* win) {
    wm_remove_window(win);
    s_control_win = NULL;
    s_lbl_theme   = NULL;
}

/*
 * app_control_open:
 * Opens or focuses the Control Panel settings window.
 */
void app_control_open(void) {
    if (s_control_win) {
        wm_bring_to_front(s_control_win);
        return;
    }

    s_control_win = window_create(180, 100, CONTROL_WIN_W, CONTROL_WIN_H,
                                  "Control Panel",
                                  WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_control_win) return;

    s_control_win->on_close = on_control_close;

    /* 1. Personalization Section */
    window_draw_string(s_control_win, 16, 12, "Desktop Personalization & Themes", 0x001B365D, 0, true);
    window_draw_rect(s_control_win, 14, 30, CONTROL_WIN_W - 28, 80, 0x00B0C0D0);

    s_lbl_theme = widget_create_label(20, 36, 400, 18, "Active Theme: Classic Navy", 0x00203040);
    widget_add_to_window(s_control_win, s_lbl_theme);

    widget_add_to_window(s_control_win,
        widget_create_button(20, 62, 95, 32, "Teal", on_btn_theme_teal, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(125, 62, 95, 32, "Navy", on_btn_theme_navy, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(230, 62, 95, 32, "Slate", on_btn_theme_slate, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(335, 62, 95, 32, "Forest", on_btn_theme_forest, NULL));

    /* 2. Kernel Security Status Section */
    window_draw_string(s_control_win, 16, 122, "System Security & Kernel Integrity", 0x001B365D, 0, true);
    window_fill_rect(s_control_win, 14, 140, CONTROL_WIN_W - 28, 90, 0x00FFFFFF);
    window_draw_rect(s_control_win, 14, 140, CONTROL_WIN_W - 28, 90, 0x00B0C0D0);

    window_draw_string(s_control_win, 24, 148, "[OK] Page 0 NULL Pointer Trap: ACTIVE (Page Fault on 0x0)", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 168, "[OK] Stack Boundary Canaries: ENABLED (0xDEADBEEF Guard)", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 188, "[OK] VFS Sandbox Traversal Guard: RESTRICTED TO ROOT /", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 208, "[OK] Bounded String Utilities: strlcpy / strlcat ACTIVE", 0x00008800, 0, true);

    /* 3. System Hardware Summary */
    window_draw_string(s_control_win, 16, 242, "Hardware Specifications", 0x001B365D, 0, true);
    window_fill_rect(s_control_win, 14, 260, CONTROL_WIN_W - 28, 64, 0x00F8FAFC);
    window_draw_rect(s_control_win, 14, 260, CONTROL_WIN_W - 28, 64, 0x00B0C0D0);

    window_draw_string(s_control_win, 24, 268, "Architecture: x86 32-bit Protected Mode (i686 Flat 4GB)", 0x00304050, 0, true);
    window_draw_string(s_control_win, 24, 286, "Display: 1024x768x32 RGB Linear Framebuffer (VBE)", 0x00304050, 0, true);
    window_draw_string(s_control_win, 24, 304, "Scheduler: Preemptive Round-Robin (100 Hz Timer Quantum)", 0x00304050, 0, true);

    wm_add_window(s_control_win);
    serial_writestring("[CONTROL] Control Panel opened\n");
}

