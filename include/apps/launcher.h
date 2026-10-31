#ifndef APPS_LAUNCHER_H
#define APPS_LAUNCHER_H

#include "lib/types.h"

/*
 * launcher.h:
 * Milestone 6 Desktop Application Launcher & Start Menu.
 *
 * Why this is needed:
 * Manages the launch of desktop productivity applications:
 * 1. Renders the classic Start Menu popup and handles item selection.
 * 2. Renders clickable desktop shortcut icons on the desktop background.
 * 3. Enforces singleton window management so opening an app either creates
 *    its window or brings an existing instance to the front.
 */

/* Start Menu dimensions */
#define LAUNCHER_MENU_WIDTH   220
#define LAUNCHER_MENU_HEIGHT  218
#define LAUNCHER_MENU_X       6
#define LAUNCHER_MENU_Y       (768 - 36 - LAUNCHER_MENU_HEIGHT) /* Above taskbar */

/* Desktop icon dimensions */
#define LAUNCHER_ICON_WIDTH   72
#define LAUNCHER_ICON_HEIGHT  76
#define LAUNCHER_ICON_START_X 24
#define LAUNCHER_ICON_START_Y 24
#define LAUNCHER_ICON_SPACING 86

/* Initializes application launcher and registers desktop shortcuts */
void launcher_init(void);

/* Checks if Start Menu popup is currently open */
bool launcher_is_start_menu_open(void);

/* Toggles Start Menu open / closed */
void launcher_toggle_start_menu(void);

/* Closes Start Menu if open */
void launcher_close_start_menu(void);

/* Renders the Start Menu popup if open (clipped to specified dirty rectangle) */
void launcher_render_start_menu(int clip_x, int clip_y, int clip_w, int clip_h);

/* Renders desktop shortcut icons on the desktop background */
void launcher_render_desktop_icons(int clip_x, int clip_y, int clip_w, int clip_h);

/*
 * Handles mouse clicks on Start Menu and desktop icons.
 * Returns true if the launcher consumed the click event.
 */
bool launcher_handle_mouse(int mx, int my, bool left_pressed);

/* App launching routines */
void launcher_launch_calc(void);
void launcher_launch_editor(void);
void launcher_launch_sheet(void);
void launcher_launch_clock(void);
void launcher_launch_sysmon(void);
void launcher_launch_about(void);

/* Checks if user requested desktop exit via Start Menu shutdown option */
bool launcher_should_exit(void);

/* Resets exit flag upon desktop entry */
void launcher_reset_exit(void);

#endif /* APPS_LAUNCHER_H */
