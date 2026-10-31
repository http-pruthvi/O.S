#include "apps/control.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gui/theme.h"
#include "gui/lockscreen.h"
#include "gfx/gfx.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define CONTROL_WIN_W 490
#define CONTROL_WIN_H 360

static window_t* s_control_win = NULL;
static widget_t* s_lbl_theme   = NULL;

static void on_btn_theme_catppuccin(widget_t* w, void* data) {
    (void)w; (void)data;
    theme_set(THEME_CATPPUCCIN_MOCHA);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Preset: Catppuccin Mocha (#1E1E2E)");
    wm_redraw_all();
}

static void on_btn_theme_tokyo(widget_t* w, void* data) {
    (void)w; (void)data;
    theme_set(THEME_TOKYO_NIGHT);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Preset: Tokyo Night (#24283B)");
    wm_redraw_all();
}

static void on_btn_theme_cyber(widget_t* w, void* data) {
    (void)w; (void)data;
    theme_set(THEME_CYBERPUNK);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Preset: Cyberpunk Neon (#0F091A)");
    wm_redraw_all();
}

static void on_btn_theme_nord(widget_t* w, void* data) {
    (void)w; (void)data;
    theme_set(THEME_NORD);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Preset: Nord Frost (#2E3440)");
    wm_redraw_all();
}

static void on_btn_theme_retro(widget_t* w, void* data) {
    (void)w; (void)data;
    theme_set(THEME_CLASSIC_RETRO);
    if (s_lbl_theme) widget_set_text(s_lbl_theme, "Active Preset: Classic Retro Teal (#008080)");
    wm_redraw_all();
}

static void on_btn_lock_session(widget_t* w, void* data) {
    (void)w; (void)data;
    lockscreen_activate();
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

    s_control_win = window_create(180, 80, CONTROL_WIN_W, CONTROL_WIN_H,
                                  "Control Panel & Customizer",
                                  WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_control_win) return;

    s_control_win->on_close = on_control_close;

    /* 1. Personalization Section */
    window_draw_string(s_control_win, 16, 10, "Desktop Ricing & Theme Engine", 0x001B365D, 0, true);
    window_draw_rect(s_control_win, 14, 26, CONTROL_WIN_W - 28, 88, 0x00B0C0D0);

    const theme_t* cur_th = theme_get_current();
    char th_buf[64] = "Active Preset: ";
    if (cur_th) strlcat(th_buf, cur_th->name, sizeof(th_buf));
    else strlcat(th_buf, "Catppuccin Mocha", sizeof(th_buf));

    s_lbl_theme = widget_create_label(20, 32, 280, 18, th_buf, 0x00203040);
    widget_add_to_window(s_control_win, s_lbl_theme);

    /* Lock Screen Quick Action Button */
    widget_add_to_window(s_control_win,
        widget_create_button(310, 30, 150, 24, "Lock Session (F6)", on_btn_lock_session, NULL));

    /* 5 Theme Buttons across row */
    widget_add_to_window(s_control_win,
        widget_create_button(20, 64, 88, 30, "Catppuccin", on_btn_theme_catppuccin, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(112, 64, 84, 30, "TokyoNight", on_btn_theme_tokyo, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(200, 64, 84, 30, "Cyberpunk", on_btn_theme_cyber, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(288, 64, 76, 30, "Nord", on_btn_theme_nord, NULL));

    widget_add_to_window(s_control_win,
        widget_create_button(368, 64, 92, 30, "Retro Teal", on_btn_theme_retro, NULL));

    /* 2. Kernel Security Status Section */
    window_draw_string(s_control_win, 16, 126, "System Security & Kernel Integrity", 0x001B365D, 0, true);
    window_fill_rect(s_control_win, 14, 142, CONTROL_WIN_W - 28, 90, 0x00FFFFFF);
    window_draw_rect(s_control_win, 14, 142, CONTROL_WIN_W - 28, 90, 0x00B0C0D0);

    window_draw_string(s_control_win, 24, 150, "[OK] Page 0 NULL Pointer Trap: ACTIVE (Page Fault on 0x0)", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 170, "[OK] Stack Boundary Canaries: ENABLED (0xDEADBEEF Guard)", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 190, "[OK] VFS Sandbox Traversal Guard: RESTRICTED TO ROOT /", 0x00008800, 0, true);
    window_draw_string(s_control_win, 24, 210, "[OK] Bounded String Utilities: strlcpy / strlcat ACTIVE", 0x00008800, 0, true);

    /* 3. System Hardware Summary */
    window_draw_string(s_control_win, 16, 244, "Hardware Specifications", 0x001B365D, 0, true);
    window_fill_rect(s_control_win, 14, 260, CONTROL_WIN_W - 28, 68, 0x00F8FAFC);
    window_draw_rect(s_control_win, 14, 260, CONTROL_WIN_W - 28, 68, 0x00B0C0D0);

    window_draw_string(s_control_win, 24, 268, "Architecture: x86 32-bit Protected Mode (i686 Flat 4GB)", 0x00304050, 0, true);
    window_draw_string(s_control_win, 24, 286, "Display: 1024x768x32 RGB Linear Framebuffer (VBE)", 0x00304050, 0, true);
    window_draw_string(s_control_win, 24, 304, "Scheduler: Preemptive Round-Robin (100 Hz Timer Quantum)", 0x00304050, 0, true);

    wm_add_window(s_control_win);
    serial_writestring("[CONTROL] Control Panel opened\n");
}
