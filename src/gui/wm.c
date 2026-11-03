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

/* Linked list of managed windows in Z-order (top-to-bottom) */
static window_t* wm_top_window    = NULL; /* Active, topmost window */
static window_t* wm_bottom_window = NULL; /* Bottom-most window */

/* Window dragging state */
static bool      wm_drag_active   = false;
static window_t* wm_drag_window   = NULL;
static int       wm_drag_offset_x = 0;
static int       wm_drag_offset_y = 0;

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

    int old_x = win->x;
    int old_y = win->y;
    int old_w = win->width;
    int old_h = win->height;

    /* If this window was being dragged, cancel drag */
    if (wm_drag_window == win) {
        wm_drag_active = false;
        wm_drag_window = NULL;
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

    /* 3. Draw Window Tabs for all open windows */
    int tab_x = 56;
    int tab_w = 150;
    int tab_h = 32;
    int tab_spacing = 4;

    /* Collect windows and draw in bottom-to-top order */
    window_t* curr = wm_bottom_window;
    while (curr) {
        if (tab_x + tab_w > WM_SCREEN_WIDTH - 220) {
            break; /* Prevent tab bar from overlapping system tray */
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
                gfx_fill_rect(tab_x + 20, btn_y + tab_h - 2, tab_w - 40, 2, 0x000078D4);
            } else if (!(curr->flags & WINDOW_FLAG_MINIMIZED)) {
                /* Subtle dot for running but unfocused window */
                gfx_fill_rect(tab_x + tab_w / 2 - 3, btn_y + tab_h - 2, 6, 2, 0x00707070);
            }

            /* Truncate title to fit tab width */
            char tab_title[18];
            size_t ti = 0;
            while (curr->title[ti] && ti < 16) {
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
    int friday_chip_x = WM_SCREEN_WIDTH - 230;
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
    int clock_x = WM_SCREEN_WIDTH - 120;
    int clock_y = WM_TASKBAR_Y + 12;
    if (clock_x < clip_r && clock_x + 115 > clip_x) {
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

        /* Draw desktop shortcut icons */
        launcher_render_desktop_icons(x, y, w, desktop_h);
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

    bool left_pressed  = (ev->buttons & MOUSE_BTN_LEFT) && !(wm_prev_buttons & MOUSE_BTN_LEFT);
    bool left_released = !(ev->buttons & MOUSE_BTN_LEFT) && (wm_prev_buttons & MOUSE_BTN_LEFT);
    bool left_down     = (ev->buttons & MOUSE_BTN_LEFT) != 0;

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

            int tab_x = 56;
            int tab_w = 150;
            int tab_spacing = 4;
            int btn_y = WM_TASKBAR_Y + 4;
            int tab_h = 32;

            window_t* curr = wm_bottom_window;
            while (curr) {
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

            if (hit_type == WINDOW_HIT_TITLE_BAR && (hit_win->flags & WINDOW_FLAG_MOVABLE)) {
                /* Initiate window dragging */
                wm_drag_active = true;
                wm_drag_window = hit_win;
                wm_drag_offset_x = mx - hit_win->x;
                wm_drag_offset_y = my - hit_win->y;
            }
        } else {
            /* D. No window hit: check desktop shortcut icons */
            if (launcher_handle_mouse(mx, my, left_pressed)) {
                wm_prev_buttons = ev->buttons;
                return;
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

    /* 3. Handle Left Click Release */
    if (left_released) {
        if (wm_drag_active) {
            wm_drag_active = false;
            wm_drag_window = NULL;
        }
    }

    /* 4. Dispatch client mouse events to the focused window widgets */
    if (!wm_drag_active && wm_top_window && (wm_top_window->flags & WINDOW_FLAG_VISIBLE)) {
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
 * Checks if a window dragging operation is currently taking place.
 */
bool wm_is_dragging(void) {
    return wm_drag_active;
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
