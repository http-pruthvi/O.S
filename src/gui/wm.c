#include "gui/wm.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gui/hud.h"
#include "gui/waybar.h"
#include "gui/dock.h"
#include "gui/startmenu.h"
#include "gui/notification.h"
#include "gui/lockscreen.h"
#include "apps/launcher.h"
#include "apps/friday.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "apps/store.h"
#include "apps/explorer.h"
#include "gfx/cursor.h"

extern void app_explorer_open(void);
extern void app_terminal_open(void);
extern void launcher_launch_editor(void);
extern void launcher_launch_calc(void);
extern void app_control_open(void);
extern void app_taskmgr_open(void);
extern void startmenu_open_search(void);


/* Linked list of managed windows in Z-order (top-to-bottom) */
static window_t* wm_top_window    = NULL; /* Active, topmost window */
static window_t* wm_bottom_window = NULL; /* Bottom-most window */

/* Window dragging state */
static bool      wm_drag_active   = false;
static window_t* wm_drag_window   = NULL;
static int       wm_drag_offset_x = 0;
static int       wm_drag_offset_y = 0;

/* Window resizing state */
static bool      wm_resize_active  = false;
static window_t* wm_resize_window  = NULL;
static int       wm_resize_edge    = WINDOW_HIT_NONE;
static int       wm_resize_start_x = 0;
static int       wm_resize_start_y = 0;
static int       wm_resize_orig_x  = 0;
static int       wm_resize_orig_y  = 0;
static int       wm_resize_orig_w  = 0;
static int       wm_resize_orig_h  = 0;

/* Mouse button tracking */
static uint8_t   wm_prev_buttons  = 0;
static uint32_t  s_wallpaper_color = WM_COLOR_DESKTOP;

/* Helper: integer min and max */
static inline int min_val(int a, int b) { return (a < b) ? a : b; }
static inline int max_val(int a, int b) { return (a > b) ? a : b; }

/*
 * wm_init:
 * Initializes the window manager and allocates the underlying graphics back buffer.
 */
bool wm_init(void) {
    if (!gfx_init()) {
        serial_writestring("[WM] Error: Failed to initialize 2D graphics back buffer!\n");
        return false;
    }

    wm_top_window = NULL;
    wm_bottom_window = NULL;
    wm_drag_active = false;
    wm_drag_window = NULL;
    wm_prev_buttons = 0;

    /* Initialize Theme, HUD, Waybar, Dock, StartMenu, Notification and Lock Screen */
    theme_init();
    hud_init();
    waybar_init();
    dock_init();
    startmenu_init();
    notification_init();
    lockscreen_init();

    serial_writestring("[WM] Window Manager initialized successfully with Catppuccin Mocha theme.\n");
    return true;
}

/*
 * wm_bring_to_front:
 * Moves a window to the top of the z-order stack and gives it active focus.
 */
void wm_bring_to_front(window_t* win) {
    if (!win) return;

    /* If already on top and focused, nothing to change */
    if (win == wm_top_window && (win->flags & WINDOW_FLAG_FOCUSED)) {
        return;
    }

    /* 1. Unfocus current top window if different */
    if (wm_top_window && wm_top_window != win) {
        wm_top_window->flags &= ~WINDOW_FLAG_FOCUSED;
    }

    /* 2. Detach win from its current position in the linked list */
    if (win != wm_top_window) {
        if (win->prev) {
            win->prev->next = win->next;
        }
        if (win->next) {
            win->next->prev = win->prev;
        }
        if (win == wm_bottom_window) {
            wm_bottom_window = win->prev;
        }

        /* 3. Insert win at the head of the list */
        win->prev = NULL;
        win->next = wm_top_window;
        if (wm_top_window) {
            wm_top_window->prev = win;
        }
        wm_top_window = win;

        if (!wm_bottom_window) {
            wm_bottom_window = win;
        }
    }

    /* 4. Mark focused and visible */
    win->flags |= (WINDOW_FLAG_VISIBLE | WINDOW_FLAG_FOCUSED);
    win->flags &= ~WINDOW_FLAG_MINIMIZED;

    /* Invalidate window region and taskbar */
    wm_invalidate_rect(win->x, win->y, win->width, win->height);
    wm_invalidate_rect(0, WM_TASKBAR_Y, WM_SCREEN_WIDTH, WM_TASKBAR_HEIGHT);
}

/*
 * wm_add_window:
 * Registers a new window with the window manager and brings it to the top.
 */
void wm_add_window(window_t* win) {
    if (!win) return;

    if (!wm_top_window) {
        wm_top_window = win;
        wm_bottom_window = win;
        win->prev = NULL;
        win->next = NULL;
        win->flags |= (WINDOW_FLAG_VISIBLE | WINDOW_FLAG_FOCUSED);
    } else {
        wm_bring_to_front(win);
    }

    wm_invalidate_rect(win->x, win->y, win->width, win->height);
    wm_invalidate_rect(0, WM_TASKBAR_Y, WM_SCREEN_WIDTH, WM_TASKBAR_HEIGHT);
}

/*
 * wm_remove_window:
 * Removes a window from the z-order stack, refocuses the next window, and destroys it.
 */
void wm_remove_window(window_t* win) {
    if (!win) return;

    /* Guard: Verify win is actually in the window manager list */
    bool found = false;
    window_t* check = wm_top_window;
    while (check) {
        if (check == win) {
            found = true;
            break;
        }
        check = check->next;
    }
    if (!found) {
        return; /* Window was already removed or never tracked */
    }

    int old_x = win->x;
    int old_y = win->y;
    int old_w = win->width;
    int old_h = win->height;

    /* If this window was being dragged or resized, cancel operation */
    if (wm_drag_window == win) {
        wm_drag_active = false;
        wm_drag_window = NULL;
    }
    if (wm_resize_window == win) {
        wm_resize_active = false;
        wm_resize_window = NULL;
    }

    /* Detach from linked list */
    if (win->prev) {
        win->prev->next = win->next;
    }
    if (win->next) {
        win->next->prev = win->prev;
    }
    if (win == wm_top_window) {
        wm_top_window = win->next;
        if (wm_top_window) {
            wm_top_window->flags |= WINDOW_FLAG_FOCUSED;
        }
    }
    if (win == wm_bottom_window) {
        wm_bottom_window = win->prev;
    }

    /* Free window resources */
    window_destroy(win);

    /* Invalidate freed area and taskbar */
    wm_invalidate_rect(old_x, old_y, old_w, old_h);
    wm_invalidate_rect(0, WM_TASKBAR_Y, WM_SCREEN_WIDTH, WM_TASKBAR_HEIGHT);
}

/*
 * wm_get_focused_window:
 * Returns the currently active window.
 */
window_t* wm_get_focused_window(void) {
    return wm_top_window;
}

/*
 * wm_toggle_minimize:
 * Minimizes a window or restores it if already minimized.
 */
void wm_toggle_minimize(window_t* win) {
    if (!win) return;

    if (win->flags & WINDOW_FLAG_MINIMIZED) {
        /* Restore */
        win->flags &= ~WINDOW_FLAG_MINIMIZED;
        wm_bring_to_front(win);
    } else {
        /* Minimize */
        win->flags |= WINDOW_FLAG_MINIMIZED;
        win->flags &= ~WINDOW_FLAG_FOCUSED;

        /* Focus next visible window if available */
        window_t* curr = wm_top_window;
        while (curr) {
            if (!(curr->flags & WINDOW_FLAG_MINIMIZED) && (curr->flags & WINDOW_FLAG_VISIBLE)) {
                wm_bring_to_front(curr);
                break;
            }
            curr = curr->next;
        }

        wm_invalidate_rect(win->x, win->y, win->width, win->height);
        wm_invalidate_rect(0, WM_TASKBAR_Y, WM_SCREEN_WIDTH, WM_TASKBAR_HEIGHT);
    }
}

/*
 * wm_render_taskbar:
 * Draws the modern Windows 11 Fluent 40px taskbar, 4-quadrant Start logo,
 * active indicator pills for running windows, Friday AI status, and clock.
 */
void wm_render_taskbar(int clip_x, int clip_y, int clip_w, int clip_h) {
    uint32_t* backbuf = gfx_get_backbuffer();
    if (!backbuf) return;

    int tb_b = WM_SCREEN_HEIGHT;
    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    /* Check if taskbar intersects clipping rect */
    if (WM_TASKBAR_Y >= clip_b || tb_b <= clip_y) {
        return;
    }

    const theme_t* th = theme_get_current();
    uint32_t tb_bg = th ? th->bg_panel : 0x001C1C1C;
    uint32_t tb_bd = 0x002E2E2E;

    /* 1. Fill Taskbar Background */
    int draw_y0 = max_val(WM_TASKBAR_Y, clip_y);
    int draw_y1 = min_val(tb_b, clip_b);

    for (int r = draw_y0; r < draw_y1; r++) {
        uint32_t* line = &backbuf[r * WM_SCREEN_WIDTH];
        for (int c = max_val(0, clip_x); c < min_val(WM_SCREEN_WIDTH, clip_r); c++) {
            line[c] = (r == WM_TASKBAR_Y) ? tb_bd : tb_bg;
        }
    }

    /* 2. Draw Windows 11 Start Button: (8, WM_TASKBAR_Y + 4, 40, 32) */
    int btn_x = 8;
    int btn_y = WM_TASKBAR_Y + 4;
    int btn_w = 40;
    int btn_h = 32;

    if (btn_x < clip_r && btn_x + btn_w > clip_x && btn_y < clip_b && btn_y + btn_h > clip_y) {
        bool sm_open = startmenu_is_open() || launcher_is_start_menu_open();
        uint32_t btn_bg = sm_open ? 0x00333333 : 0x00242424;
        gfx_fill_rect(btn_x, btn_y, btn_w, btn_h, btn_bg);
        gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, sm_open ? 0x000078D4 : 0x00383838);

        /* Iconic Windows 11 4-Quadrant Logo (6x6 each, 2px gap) */
        uint32_t logo_col = sm_open ? 0x0060A5FA : 0x000078D4;
        int lx = btn_x + 13;
        int ly = btn_y + 9;
        /* Top-Left */
        gfx_fill_rect(lx, ly, 6, 6, logo_col);
        /* Top-Right */
        gfx_fill_rect(lx + 8, ly, 6, 6, logo_col);
        /* Bottom-Left */
        gfx_fill_rect(lx, ly + 8, 6, 6, logo_col);
        /* Bottom-Right */
        gfx_fill_rect(lx + 8, ly + 8, 6, 6, logo_col);
    }

    /* 2.5 Windows 11 Search Button / Box: (52, WM_TASKBAR_Y + 4, 110, 32) */
    int srch_x = 52;
    int srch_w = 110;
    if (srch_x < clip_r && srch_x + srch_w > clip_x && btn_y < clip_b && btn_y + btn_h > clip_y) {
        gfx_fill_rect(srch_x, btn_y, srch_w, btn_h, 0x00222222);
        gfx_draw_rect(srch_x, btn_y, srch_w, btn_h, 0x003A3A3A);

        /* Magnifying Glass Icon */
        int sx = srch_x + 12;
        int sy = btn_y + 11;
        gfx_draw_rect(sx, sy, 7, 7, 0x00A0A0A0);
        gfx_draw_line(sx + 6, sy + 6, sx + 9, sy + 9, 0x00A0A0A0);

        /* Search Label */
        gfx_draw_string(srch_x + 28, btn_y + 8, "Search", 0x00A0A0A0, 0x00222222, true);
    }

    /* Vertical Divider between Search and Pinned Icons */
    if (168 >= clip_x && 168 < clip_r) {
        gfx_fill_rect(168, WM_TASKBAR_Y + 8, 1, 24, 0x002E2E2E);
    }

    /* 2.8 Windows 11 Pinned App Icons (Files, Terminal, Editor, Calc, Store, Settings, TaskMgr) */
    int pin_x = 174;
    int pin_w = 34;
    int pin_spacing = 4;

    for (int p = 0; p < 7; p++) {
        int px = pin_x + p * (pin_w + pin_spacing);
        if (px < clip_r && px + pin_w > clip_x && btn_y < clip_b && btn_y + btn_h > clip_y) {
            gfx_fill_rect(px, btn_y, pin_w, btn_h, 0x00222222);
            gfx_draw_rect(px, btn_y, pin_w, btn_h, 0x002D2D2D);

            switch (p) {
                case 0: /* Files: Manila Folder */
                    gfx_fill_rect(px + 7, btn_y + 7, 8, 3, 0x00D49B00);
                    gfx_fill_rect(px + 6, btn_y + 10, 22, 14, 0x00F5BA13);
                    gfx_draw_rect(px + 6, btn_y + 10, 22, 14, 0x00A06D00);
                    gfx_fill_rect(px + 10, btn_y + 8, 12, 3, 0x00FFFFFF);
                    break;
                case 1: /* Terminal: Blue Console */
                    gfx_fill_rect(px + 6, btn_y + 7, 22, 18, 0x000F172A);
                    gfx_draw_rect(px + 6, btn_y + 7, 22, 18, 0x0000E5FF);
                    gfx_draw_char(px + 9, btn_y + 8, '>', 0x0000E5FF, 0x000F172A, false);
                    gfx_fill_rect(px + 18, btn_y + 19, 5, 2, 0x0000E5FF);
                    break;
                case 2: /* Editor: Notepad Document */
                    gfx_fill_rect(px + 8, btn_y + 6, 18, 20, 0x00FAFAFA);
                    gfx_draw_rect(px + 8, btn_y + 6, 18, 20, 0x0060A5FA);
                    gfx_fill_rect(px + 18, btn_y + 6, 8, 6, 0x0093C5FD);
                    gfx_fill_rect(px + 11, btn_y + 12, 10, 2, 0x002563EB);
                    gfx_fill_rect(px + 11, btn_y + 16, 12, 2, 0x002563EB);
                    break;
                case 3: /* Calculator: LCD + Buttons */
                    gfx_fill_rect(px + 7, btn_y + 6, 20, 20, 0x00334155);
                    gfx_draw_rect(px + 7, btn_y + 6, 20, 20, 0x0094A3B8);
                    gfx_fill_rect(px + 9, btn_y + 8, 16, 5, 0x0086EFAC);
                    for (int r = 0; r < 2; r++) {
                        for (int c = 0; c < 2; c++) {
                            gfx_fill_rect(px + 10 + c * 8, btn_y + 15 + r * 5, 5, 3, 0x00E2E8F0);
                        }
                    }
                    break;
                case 4: /* Web App Store: Shopping Bag with Gift Tag */
                    gfx_fill_rect(px + 7, btn_y + 9, 20, 17, 0x000078D4);
                    gfx_draw_rect(px + 7, btn_y + 9, 20, 17, 0x0038BDF8);
                    gfx_draw_rect(px + 11, btn_y + 5, 12, 5, 0x00FFFFFF);
                    gfx_fill_rect(px + 14, btn_y + 14, 6, 6, 0x00FDE047);
                    break;
                case 5: /* Settings: Purple Gear */
                    gfx_fill_rect(px + 8, btn_y + 8, 18, 18, 0x006D28D9);
                    gfx_draw_rect(px + 8, btn_y + 8, 18, 18, 0x00C4B5FD);
                    gfx_fill_rect(px + 14, btn_y + 14, 6, 6, 0x001E1B4B);
                    break;
                case 6: /* Task Manager: Activity Graph */
                    gfx_fill_rect(px + 6, btn_y + 7, 22, 18, 0x001E293B);
                    gfx_draw_rect(px + 6, btn_y + 7, 22, 18, 0x0064748B);
                    gfx_fill_rect(px + 9, btn_y + 17, 4, 6, 0x0022C55E);
                    gfx_fill_rect(px + 15, btn_y + 12, 4, 11, 0x0022C55E);
                    gfx_fill_rect(px + 21, btn_y + 14, 4, 9, 0x0022C55E);
                    break;
            }
        }
    }

    /* Vertical Divider between Pinned Icons and Window Tabs */
    if (442 >= clip_x && 442 < clip_r) {
        gfx_fill_rect(442, WM_TASKBAR_Y + 8, 1, 24, 0x002E2E2E);
    }

    /* 3. Draw Window Tabs for all open windows (Starting at x = 448) */
    int tab_x = 448;
    int tab_w = 120;
    int tab_h = 32;
    int tab_spacing = 4;

    /* Collect windows and draw in bottom-to-top order */
    window_t* curr = wm_bottom_window;
    while (curr) {
        if (tab_x + tab_w > WM_SCREEN_WIDTH - 260) {
            break; /* Clean margin before Friday AI tray chip at 774 */
        }

        if (tab_x < clip_r && tab_x + tab_w > clip_x && btn_y < clip_b && btn_y + tab_h > clip_y) {
            bool focused = (curr == wm_top_window) && !(curr->flags & WINDOW_FLAG_MINIMIZED);
            uint32_t tab_bg = focused ? 0x002B2B2B : 0x00202020;
            uint32_t tab_bd = focused ? 0x003A3A3A : 0x00282828;
            uint32_t tab_fg = focused ? 0x00FFFFFF : 0x00A0A0A0;

            gfx_fill_rect(tab_x, btn_y, tab_w, tab_h, tab_bg);
            gfx_draw_rect(tab_x, btn_y, tab_w, tab_h, tab_bd);

            /* Windows 11 Running Indicator Pill (2px accent blue under focused tab) */
            if (focused) {
                gfx_fill_rect(tab_x + 16, btn_y + tab_h - 2, tab_w - 32, 2, 0x000078D4);
            } else if (!(curr->flags & WINDOW_FLAG_MINIMIZED)) {
                /* Subtle dot for running but unfocused window */
                gfx_fill_rect(tab_x + tab_w / 2 - 3, btn_y + tab_h - 2, 6, 2, 0x00707070);
            }

            /* Truncate title to fit tab width */
            char tab_title[14];
            size_t ti = 0;
            while (curr->title[ti] && ti < 12) {
                tab_title[ti] = curr->title[ti];
                ti++;
            }
            tab_title[ti] = '\0';

            gfx_draw_string(tab_x + 8, btn_y + 8, tab_title, tab_fg, tab_bg, true);
        }

        tab_x += tab_w + tab_spacing;
        curr = curr->prev;
    }

    /* 4. Draw System Tray (Right side) */
    /* Friday AI System Tray Status Chip */
    int friday_chip_x = WM_SCREEN_WIDTH - 250;
    int friday_chip_y = WM_TASKBAR_Y + 7;
    int friday_chip_w = 98;
    int friday_chip_h = 26;
    if (friday_chip_x < clip_r && friday_chip_x + friday_chip_w > clip_x) {
        gfx_fill_rect(friday_chip_x, friday_chip_y, friday_chip_w, friday_chip_h, 0x00242424);
        gfx_draw_rect(friday_chip_x, friday_chip_y, friday_chip_w, friday_chip_h, 0x00383838);
        /* Green online dot */
        gfx_fill_rect(friday_chip_x + 8, friday_chip_y + 10, 6, 6, 0x00107C41);
        gfx_draw_string(friday_chip_x + 20, friday_chip_y + 5, "Friday AI", 0x00D0D0D0, 0x00242424, true);
    }

    /* System Tray Digital Clock */
    int clock_x = WM_SCREEN_WIDTH - 140;
    int clock_y = WM_TASKBAR_Y + 12;
    if (clock_x < clip_r && clock_x + 130 > clip_x) {
        char clock_buf[16];
        timer_format_clock(clock_buf);
        gfx_draw_string(clock_x, clock_y, clock_buf, 0x00FFFFFF, tb_bg, true);
    }
}

/*
 * wm_invalidate_rect:
 * Re-composites a dirty region of the desktop:
 * 1. Desktop background & centered watermark.
 * 2. Windows in bottom-to-top Z-order (Painter's Algorithm).
 * 3. Bottom taskbar.
 * 4. Flushes the composited rectangle to VRAM (handling cursor overlap).
 */
void wm_invalidate_rect(int x, int y, int w, int h) {
    uint32_t* backbuf = gfx_get_backbuffer();
    if (!backbuf || w <= 0 || h <= 0) return;

    /* Clip dirty rectangle to screen boundaries */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > WM_SCREEN_WIDTH)  w = WM_SCREEN_WIDTH - x;
    if (y + h > WM_SCREEN_HEIGHT) h = WM_SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    /* 1. Clear Desktop Area in dirty region */
    int desktop_h = min_val(y + h, WM_TASKBAR_Y) - y;
    if (desktop_h > 0) {
        gfx_fill_rect(x, y, w, desktop_h, s_wallpaper_color);

        /* Only render Conky HUD if not in clean Windows 11 theme */
        if (theme_get_current_id() != THEME_WINDOWS_11) {
            hud_render(x, y, w, desktop_h);
        }

        /* Draw desktop shortcut icons only for Linux desktop themes */
        if (theme_get_current_id() != THEME_WINDOWS_11) {
            launcher_render_desktop_icons(x, y, w, desktop_h);
        }
    }

    /* 2. Composite Windows from bottom to top (Painter's Algorithm) */
    window_t* curr = wm_bottom_window;
    while (curr) {
        if ((curr->flags & WINDOW_FLAG_VISIBLE) && !(curr->flags & WINDOW_FLAG_MINIMIZED)) {
            window_render(curr, backbuf, WM_SCREEN_WIDTH, x, y, w, h);
        }
        curr = curr->prev;
    }

    /* 2.5 Composite Friday AI Companion Orb (only if not Windows 11) */
    if (theme_get_current_id() != THEME_WINDOWS_11) {
        friday_render_orb(backbuf, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
    }

    /* 3. Composite Top Waybar Status Bar (only for Linux themes) */
    if (theme_get_current_id() != THEME_WINDOWS_11) {
        waybar_render(x, y, w, h);
    }

    /* 4. Composite Floating Pill Dock (only for Linux themes) */
    if (theme_get_current_id() != THEME_WINDOWS_11) {
        dock_render(x, y, w, h);
    }

    /* 5. Composite Taskbar if dirty rect touches the bottom */
    if (y + h > WM_TASKBAR_Y) {
        wm_render_taskbar(x, y, w, h);
    }

    /* 6. Composite Modern Start Menu popup if open */
    if (startmenu_is_open()) {
        startmenu_render(backbuf, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT, theme_get_current());
    }

    /* 7. Composite Toast Notifications if active */
    notification_render(backbuf, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT, theme_get_current());

    /* 7.5 Composite File Explorer Context Menu popup if active */
    if (explorer_context_menu_is_open()) {
        explorer_context_menu_render(backbuf, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
    }

    /* 8. Composite Legacy Start Menu if open and intersecting dirty rect */
    if (launcher_is_start_menu_open()) {
        launcher_render_start_menu(x, y, w, h);
    }

    /* 9. Composite Modal Lock Screen if active */
    if (lockscreen_is_active()) {
        lockscreen_render();
    }

    /* 8. Flush to Video Memory */
    gfx_flush_rect(x, y, w, h);
}

/*
 * wm_redraw_all:
 * Forces a full screen recomposite and display flush.
 */
void wm_redraw_all(void) {
    wm_invalidate_rect(0, 0, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
}

/*
 * wm_handle_mouse:
 * Core mouse input dispatcher for window dragging, focus, and controls.
 */
void wm_handle_mouse(const mouse_event_t* ev, int mx, int my) {
    if (!ev) return;

    /* Modal Lock Screen intercepts all mouse events */
    if (lockscreen_is_active()) {
        wm_prev_buttons = ev->buttons;
        return;
    }

    bool left_pressed   = (ev->buttons & MOUSE_BTN_LEFT) && !(wm_prev_buttons & MOUSE_BTN_LEFT);
    bool left_released  = !(ev->buttons & MOUSE_BTN_LEFT) && (wm_prev_buttons & MOUSE_BTN_LEFT);
    bool left_down      = (ev->buttons & MOUSE_BTN_LEFT) != 0;

    bool right_pressed  = (ev->buttons & MOUSE_BTN_RIGHT) && !(wm_prev_buttons & MOUSE_BTN_RIGHT);
    bool right_released = !(ev->buttons & MOUSE_BTN_RIGHT) && (wm_prev_buttons & MOUSE_BTN_RIGHT);
    bool right_down     = (ev->buttons & MOUSE_BTN_RIGHT) != 0;

    (void)right_released;
    (void)right_down;

    /* Dynamic Cursor Shape Update on hover / active resize */
    if (wm_resize_active) {
        if (wm_resize_edge == WINDOW_HIT_RESIZE_LEFT || wm_resize_edge == WINDOW_HIT_RESIZE_RIGHT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_H);
        } else if (wm_resize_edge == WINDOW_HIT_RESIZE_BOTTOM) {
            cursor_set_type(CURSOR_TYPE_RESIZE_V);
        } else if (wm_resize_edge == WINDOW_HIT_RESIZE_BOTTOM_RIGHT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_DIAG_BR);
        } else if (wm_resize_edge == WINDOW_HIT_RESIZE_BOTTOM_LEFT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_DIAG_BL);
        }
    } else if (wm_drag_active) {
        cursor_set_type(CURSOR_TYPE_POINTER);
    } else if (wm_top_window && (wm_top_window->flags & WINDOW_FLAG_VISIBLE) && !(wm_top_window->flags & WINDOW_FLAG_MINIMIZED)) {
        window_hit_t hover_hit = window_hit_test(wm_top_window, mx, my);
        if (hover_hit == WINDOW_HIT_RESIZE_LEFT || hover_hit == WINDOW_HIT_RESIZE_RIGHT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_H);
        } else if (hover_hit == WINDOW_HIT_RESIZE_BOTTOM) {
            cursor_set_type(CURSOR_TYPE_RESIZE_V);
        } else if (hover_hit == WINDOW_HIT_RESIZE_BOTTOM_RIGHT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_DIAG_BR);
        } else if (hover_hit == WINDOW_HIT_RESIZE_BOTTOM_LEFT) {
            cursor_set_type(CURSOR_TYPE_RESIZE_DIAG_BL);
        } else {
            cursor_set_type(CURSOR_TYPE_POINTER);
        }
    } else {
        cursor_set_type(CURSOR_TYPE_POINTER);
    }

    /* Intercept clicks if File Explorer Context Menu is open */
    if (explorer_context_menu_is_open()) {
        if (left_pressed || right_pressed) {
            if (explorer_context_menu_handle_click(mx, my, right_pressed)) {
                wm_prev_buttons = ev->buttons;
                return;
            }
        }
    }

    /* 0. Handle Right Click Down */
    if (right_pressed) {
        window_t* curr = wm_top_window;
        while (curr) {
            if ((curr->flags & WINDOW_FLAG_VISIBLE) && !(curr->flags & WINDOW_FLAG_MINIMIZED)) {
                window_hit_t hit = window_hit_test(curr, mx, my);
                if (hit == WINDOW_HIT_CLIENT) {
                    if (curr != wm_top_window) {
                        wm_bring_to_front(curr);
                    }
                    if (curr->on_right_click) {
                        int rx = mx - curr->client_x;
                        int ry = my - curr->client_y;
                        curr->on_right_click(curr, rx, ry);
                        wm_prev_buttons = ev->buttons;
                        return;
                    }
                    break;
                }
            }
            curr = curr->next;
        }
    }

    /* 1. Handle Left Click Down */
    if (left_pressed) {
        /* Check Toast Notification click */
        if (notification_handle_click(mx, my)) {
            wm_redraw_all();
            wm_prev_buttons = ev->buttons;
            return;
        }

        /* Check Modern Start Menu click if open */
        if (startmenu_is_open()) {
            if (startmenu_handle_click(mx, my)) {
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
        }

        /* A. Check Top Waybar and Floating Dock clicks (only if not Windows 11) */
        if (theme_get_current_id() != THEME_WINDOWS_11) {
            if (waybar_handle_mouse(mx, my, left_pressed)) {
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (dock_handle_mouse(mx, my, left_pressed)) {
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
        }

        /* C. Handle open Start Menu clicks */
        if (launcher_is_start_menu_open()) {
            if (launcher_handle_mouse(mx, my, left_pressed)) {
                wm_prev_buttons = ev->buttons;
                return;
            }
        }

        /* B. Check Taskbar clicks (y >= WM_TASKBAR_Y) */
        if (my >= WM_TASKBAR_Y) {
            /* Start button check (x: 8 to 48) */
            if (mx >= 8 && mx < 48) {
                startmenu_toggle();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }

            /* Search button check (x: 52 to 164) */
            if (mx >= 52 && mx < 164) {
                startmenu_open_search();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }

            /* Pinned App Icons on Start Bar */
            if (mx >= 174 && mx < 208) {
                app_explorer_open();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 212 && mx < 246) {
                app_terminal_open();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 250 && mx < 284) {
                launcher_launch_editor();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 288 && mx < 322) {
                launcher_launch_calc();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 326 && mx < 360) {
                app_store_open();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 364 && mx < 398) {
                app_control_open();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }
            if (mx >= 402 && mx < 436) {
                app_taskmgr_open();
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }

            /* Window Tabs (Starting at x = 448) */
            int tab_x = 448;
            int tab_w = 120;
            int tab_spacing = 4;
            int btn_y = WM_TASKBAR_Y + 4;
            int tab_h = 32;

            window_t* curr = wm_bottom_window;
            while (curr) {
                if (tab_x + tab_w > WM_SCREEN_WIDTH - 240) break;
                if (mx >= tab_x && mx < tab_x + tab_w && my >= btn_y && my < btn_y + tab_h) {
                    wm_toggle_minimize(curr);
                    wm_prev_buttons = ev->buttons;
                    return;
                }
                tab_x += tab_w + tab_spacing;
                curr = curr->prev;
            }

            wm_prev_buttons = ev->buttons;
            return;
        }

        /* C. Check Window Hit Testing (from top window to bottom window) */
        window_t* hit_win = NULL;
        window_hit_t hit_type = WINDOW_HIT_NONE;

        window_t* curr = wm_top_window;
        while (curr) {
            if ((curr->flags & WINDOW_FLAG_VISIBLE) && !(curr->flags & WINDOW_FLAG_MINIMIZED)) {
                window_hit_t hit = window_hit_test(curr, mx, my);
                if (hit != WINDOW_HIT_NONE) {
                    hit_win = curr;
                    hit_type = hit;
                    break;
                }
            }
            curr = curr->next;
        }

        if (hit_win) {
            /* Bring window to front if not already topmost */
            if (hit_win != wm_top_window) {
                wm_bring_to_front(hit_win);
            }

            if (hit_type == WINDOW_HIT_CLOSE_BUTTON) {
                if (hit_win->on_close) {
                    hit_win->on_close(hit_win);
                } else {
                    wm_remove_window(hit_win);
                }
                wm_prev_buttons = ev->buttons;
                return;
            }

            if (hit_type == WINDOW_HIT_MIN_BUTTON) {
                wm_toggle_minimize(hit_win);
                wm_prev_buttons = ev->buttons;
                return;
            }

            if (hit_type == WINDOW_HIT_MAX_BUTTON) {
                if (!hit_win->is_maximized) {
                    /* Save current geometry to restore later */
                    hit_win->restore_x = hit_win->x;
                    hit_win->restore_y = hit_win->y;
                    hit_win->restore_width = hit_win->width;
                    hit_win->restore_height = hit_win->height;

                    int old_x = hit_win->x;
                    int old_y = hit_win->y;
                    int old_w = hit_win->width;
                    int old_h = hit_win->height;

                    /* Maximize: fill desktop above taskbar */
                    hit_win->x = 0;
                    hit_win->y = 0;
                    hit_win->client_x = WINDOW_BORDER_WIDTH;
                    hit_win->client_y = WINDOW_TITLE_BAR_HEIGHT;
                    hit_win->is_maximized = true;

                    window_resize(hit_win, WM_SCREEN_WIDTH, WM_TASKBAR_Y);
                    wm_invalidate_rect(old_x, old_y, old_w, old_h);
                    wm_invalidate_rect(0, 0, WM_SCREEN_WIDTH, WM_TASKBAR_Y);
                } else {
                    int old_x = hit_win->x;
                    int old_y = hit_win->y;
                    int old_w = hit_win->width;
                    int old_h = hit_win->height;

                    /* Restore to original window dimensions */
                    hit_win->x = hit_win->restore_x;
                    hit_win->y = hit_win->restore_y;
                    hit_win->client_x = hit_win->restore_x + WINDOW_BORDER_WIDTH;
                    hit_win->client_y = hit_win->restore_y + WINDOW_TITLE_BAR_HEIGHT;
                    hit_win->is_maximized = false;

                    window_resize(hit_win, hit_win->restore_width, hit_win->restore_height);
                    wm_invalidate_rect(old_x, old_y, old_w, old_h);
                    wm_invalidate_rect(hit_win->x, hit_win->y, hit_win->width, hit_win->height);
                }
                wm_redraw_all();
                wm_prev_buttons = ev->buttons;
                return;
            }

            /* Initiate interactive window border resizing */
            if (hit_type == WINDOW_HIT_RESIZE_LEFT ||
                hit_type == WINDOW_HIT_RESIZE_RIGHT ||
                hit_type == WINDOW_HIT_RESIZE_BOTTOM ||
                hit_type == WINDOW_HIT_RESIZE_BOTTOM_RIGHT ||
                hit_type == WINDOW_HIT_RESIZE_BOTTOM_LEFT) {
                wm_resize_active  = true;
                wm_resize_window  = hit_win;
                wm_resize_edge    = hit_type;
                wm_resize_start_x = mx;
                wm_resize_start_y = my;
                wm_resize_orig_x  = hit_win->x;
                wm_resize_orig_y  = hit_win->y;
                wm_resize_orig_w  = hit_win->width;
                wm_resize_orig_h  = hit_win->height;
                wm_prev_buttons = ev->buttons;
                return;
            }

            if (hit_type == WINDOW_HIT_TITLE_BAR && (hit_win->flags & WINDOW_FLAG_MOVABLE) && !hit_win->is_maximized) {
                /* Initiate window dragging */
                wm_drag_active = true;
                wm_drag_window = hit_win;
                wm_drag_offset_x = mx - hit_win->x;
                wm_drag_offset_y = my - hit_win->y;
            }
        } else {
            /* D. No window hit: check desktop shortcut icons only for Linux themes */
            if (theme_get_current_id() != THEME_WINDOWS_11) {
                if (launcher_handle_mouse(mx, my, left_pressed)) {
                    wm_prev_buttons = ev->buttons;
                    return;
                }
            }
        }
    }

    /* 2. Handle Mouse Movement while Dragging */
    if (wm_drag_active && wm_drag_window && left_down) {
        int new_x = mx - wm_drag_offset_x;
        int new_y = my - wm_drag_offset_y;

        /* Clamp window to desktop boundary */
        if (new_x < 0) new_x = 0;
        if (new_x + wm_drag_window->width > WM_SCREEN_WIDTH) {
            new_x = WM_SCREEN_WIDTH - wm_drag_window->width;
        }
        if (new_y < 0) new_y = 0;
        if (new_y + wm_drag_window->height > WM_TASKBAR_Y) {
            new_y = WM_TASKBAR_Y - wm_drag_window->height;
        }

        if (new_x != wm_drag_window->x || new_y != wm_drag_window->y) {
            /* Compute union bounding box of old and new rectangles */
            int old_x = wm_drag_window->x;
            int old_y = wm_drag_window->y;
            int w = wm_drag_window->width;
            int h = wm_drag_window->height;

            int min_x = min_val(old_x, new_x);
            int min_y = min_val(old_y, new_y);
            int max_x = max_val(old_x + w, new_x + w);
            int max_y = max_val(old_y + h, new_y + h);

            /* Update coordinates */
            wm_drag_window->x = new_x;
            wm_drag_window->y = new_y;
            wm_drag_window->client_x = new_x + WINDOW_BORDER_WIDTH;
            wm_drag_window->client_y = new_y + WINDOW_TITLE_BAR_HEIGHT;

            /* Invalidate dirty bounding box */
            wm_invalidate_rect(min_x, min_y, max_x - min_x, max_y - min_y);
        }
    }

    /* 2.5 Handle Mouse Movement while Resizing */
    if (wm_resize_active && wm_resize_window && left_down) {
        int dx = mx - wm_resize_start_x;
        int dy = my - wm_resize_start_y;
        int min_w = 260;
        int min_h = 180;

        int new_x = wm_resize_orig_x;
        int new_y = wm_resize_orig_y;
        int new_w = wm_resize_orig_w;
        int new_h = wm_resize_orig_h;

        switch (wm_resize_edge) {
            case WINDOW_HIT_RESIZE_RIGHT:
                new_w = wm_resize_orig_w + dx;
                if (new_x + new_w > WM_SCREEN_WIDTH) new_w = WM_SCREEN_WIDTH - new_x;
                if (new_w < min_w) new_w = min_w;
                break;
            case WINDOW_HIT_RESIZE_BOTTOM:
                new_h = wm_resize_orig_h + dy;
                if (new_y + new_h > WM_TASKBAR_Y) new_h = WM_TASKBAR_Y - new_y;
                if (new_h < min_h) new_h = min_h;
                break;
            case WINDOW_HIT_RESIZE_BOTTOM_RIGHT:
                new_w = wm_resize_orig_w + dx;
                if (new_x + new_w > WM_SCREEN_WIDTH) new_w = WM_SCREEN_WIDTH - new_x;
                if (new_w < min_w) new_w = min_w;
                new_h = wm_resize_orig_h + dy;
                if (new_y + new_h > WM_TASKBAR_Y) new_h = WM_TASKBAR_Y - new_y;
                if (new_h < min_h) new_h = min_h;
                break;
            case WINDOW_HIT_RESIZE_LEFT:
                new_x = wm_resize_orig_x + dx;
                if (new_x < 0) new_x = 0;
                if (new_x > (wm_resize_orig_x + wm_resize_orig_w) - min_w) {
                    new_x = (wm_resize_orig_x + wm_resize_orig_w) - min_w;
                }
                new_w = (wm_resize_orig_x + wm_resize_orig_w) - new_x;
                break;
            case WINDOW_HIT_RESIZE_BOTTOM_LEFT:
                new_x = wm_resize_orig_x + dx;
                if (new_x < 0) new_x = 0;
                if (new_x > (wm_resize_orig_x + wm_resize_orig_w) - min_w) {
                    new_x = (wm_resize_orig_x + wm_resize_orig_w) - min_w;
                }
                new_w = (wm_resize_orig_x + wm_resize_orig_w) - new_x;
                new_h = wm_resize_orig_h + dy;
                if (new_y + new_h > WM_TASKBAR_Y) new_h = WM_TASKBAR_Y - new_y;
                if (new_h < min_h) new_h = min_h;
                break;
            default:
                break;
        }

        if (new_w != wm_resize_window->width || new_h != wm_resize_window->height ||
            new_x != wm_resize_window->x     || new_y != wm_resize_window->y) {
            int old_x = wm_resize_window->x;
            int old_y = wm_resize_window->y;
            int old_w = wm_resize_window->width;
            int old_h = wm_resize_window->height;

            wm_resize_window->x = new_x;
            wm_resize_window->y = new_y;
            wm_resize_window->client_x = new_x + WINDOW_BORDER_WIDTH;
            wm_resize_window->client_y = new_y + WINDOW_TITLE_BAR_HEIGHT;

            window_resize(wm_resize_window, new_w, new_h);

            int min_rx = min_val(old_x, new_x);
            int min_ry = min_val(old_y, new_y);
            int max_rx = max_val(old_x + old_w, new_x + new_w);
            int max_ry = max_val(old_y + old_h, new_y + new_h);
            wm_invalidate_rect(min_rx, min_ry, max_rx - min_rx, max_ry - min_ry);
        }
    }

    /* 3. Handle Left Click Release */
    if (left_released) {
        if (wm_drag_active) {
            wm_drag_active = false;
            wm_drag_window = NULL;
        }
        if (wm_resize_active) {
            wm_resize_active = false;
            wm_resize_window = NULL;
            cursor_set_type(CURSOR_TYPE_POINTER);
        }
    }

    /* 4. Dispatch client mouse events to the focused window widgets */
    if (!wm_drag_active && !wm_resize_active && wm_top_window && (wm_top_window->flags & WINDOW_FLAG_VISIBLE)) {
        int rx = mx - wm_top_window->client_x;
        int ry = my - wm_top_window->client_y;
        if (left_pressed && wm_top_window->on_click) {
            wm_top_window->on_click(wm_top_window, rx, ry);
        }
        widget_dispatch_mouse(wm_top_window, rx, ry, left_down, left_pressed, left_released);
    }

    wm_prev_buttons = ev->buttons;
}

/*
 * wm_handle_keyboard:
 * Routes keystrokes to the focused window.
 */
void wm_handle_keyboard(char c) {
    if (wm_top_window && (wm_top_window->flags & WINDOW_FLAG_VISIBLE)) {
        if (wm_top_window->on_key) {
            wm_top_window->on_key(wm_top_window, c);
        } else {
            widget_dispatch_key(wm_top_window, c);
        }
    }
}

/*
 * wm_is_dragging:
 * Checks if a window dragging or resizing operation is currently taking place.
 */
bool wm_is_dragging(void) {
    return wm_drag_active || wm_resize_active;
}

/*
 * wm_set_wallpaper_color:
 * Updates the desktop background color and triggers full desktop re-compositing.
 */
void wm_set_wallpaper_color(uint32_t color) {
    s_wallpaper_color = color;
    wm_redraw_all();
}

/*
 * wm_get_wallpaper_color:
 * Returns the currently active wallpaper color.
 */
uint32_t wm_get_wallpaper_color(void) {
    return s_wallpaper_color;
}
