#ifndef GUI_STARTMENU_H
#define GUI_STARTMENU_H

#include "lib/types.h"
#include "gui/theme.h"

/*
 * startmenu.h:
 * Modern Start Menu / App Launcher popup for MyOS.
 *
 * Drops down from the top-left Waybar brand chip [ MyOS ], providing quick access
 * to all desktop productivity apps, system settings, retro games, and power controls.
 */

/* Initializes the Start Menu state */
void startmenu_init(void);

/* Toggles Start Menu visibility */
void startmenu_toggle(void);

/* Opens the Start Menu */
void startmenu_open(void);

/* Closes the Start Menu */
void startmenu_close(void);

/* Returns whether the Start Menu is currently open/visible */
bool startmenu_is_open(void);

/*
 * startmenu_handle_click:
 * Handles mouse clicks within the desktop. Returns true if the click was consumed by the Start Menu.
 */
bool startmenu_handle_click(int32_t mx, int32_t my);

/*
 * startmenu_render:
 * Renders the Start Menu popup into the provided 32-bit framebuffer backbuffer.
 */
void startmenu_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme);

#endif /* GUI_STARTMENU_H */
