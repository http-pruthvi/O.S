#ifndef GUI_WM_H
#define GUI_WM_H

#include "gui/window.h"
#include "drivers/mouse.h"

/*
 * wm.h:
 * Desktop Window Manager and Z-Order Compositor.
 *
 * Why this is needed:
 * The Window Manager oversees all open windows on the system. It handles:
 * 1. Z-order stacking (topmost active window to background windows).
 * 2. Painter's algorithm compositing into the off-screen back buffer.
 * 3. Mouse-driven window dragging with automatic desktop boundary clamping.
 * 4. Focus switching when windows are clicked.
 * 5. Desktop background and bottom Taskbar with application tabs and uptime clock.
 */

#define WM_SCREEN_WIDTH     1024
#define WM_SCREEN_HEIGHT    768
#define WM_TASKBAR_HEIGHT   40
#define WM_TASKBAR_Y        (WM_SCREEN_HEIGHT - WM_TASKBAR_HEIGHT)

#define WM_COLOR_DESKTOP    0x0017263C
#define WM_COLOR_TASKBAR    0x0013171F
#define WM_COLOR_TASKBAR_BD 0x002A303C
#define WM_COLOR_START_BTN  0x0020487C
#define WM_COLOR_START_TXT  0x00FFFFFF

/* Initializes the window manager subsystem and desktop canvas */
bool wm_init(void);

/* Adds a window to the window manager and brings it to the top */
void wm_add_window(window_t* win);

/* Removes a window from the window manager and destroys it */
void wm_remove_window(window_t* win);

/* Brings a window to the top of the z-order stack and focuses it */
void wm_bring_to_front(window_t* win);

/* Returns the currently focused topmost window */
window_t* wm_get_focused_window(void);

/* Minimizes or restores a window */
void wm_toggle_minimize(window_t* win);

/* Redraws the entire desktop scene and flushes to display */
void wm_redraw_all(void);

/* Re-composites a specific dirty region of the desktop */
void wm_invalidate_rect(int x, int y, int w, int h);

/* Dispatches mouse movement and button events to window manager and windows */
void wm_handle_mouse(const mouse_event_t* ev, int mx, int my);

/* Dispatches keyboard characters to the active window */
void wm_handle_keyboard(char c);

/* Renders bottom taskbar with Start button, window tabs, and system tray clock */
void wm_render_taskbar(int clip_x, int clip_y, int clip_w, int clip_h);

/* Checks if a modal or window drag operation is currently active */
bool wm_is_dragging(void);

/* Sets desktop wallpaper background color and redraws */
void wm_set_wallpaper_color(uint32_t color);

/* Returns currently active wallpaper color */
uint32_t wm_get_wallpaper_color(void);

#endif /* GUI_WM_H */
