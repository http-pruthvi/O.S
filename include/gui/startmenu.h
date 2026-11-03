#ifndef GUI_STARTMENU_H
#define GUI_STARTMENU_H

#include "lib/types.h"
#include "gui/theme.h"

/*
 * startmenu.h:
 * Modern Start Menu & Windows Search Flyout for MyOS.
 *
 * Provides quick access to all desktop productivity apps, system settings,
 * Web App Store, retro games, power controls, and real-time live app search.
 */

/* Initializes the Start Menu state */
void startmenu_init(void);

/* Toggles Start Menu visibility */
void startmenu_toggle(void);

/* Opens the Start Menu */
void startmenu_open(void);

/* Opens the Start Menu directly in Search mode with text cursor focused */
void startmenu_open_search(void);

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
 * startmenu_handle_key:
 * Handles keyboard typing when the Start Menu / Search is open.
 * Returns true if the key was consumed.
 */
bool startmenu_handle_key(char c);

/*
 * startmenu_register_app:
 * Dynamically registers a newly installed web app so it immediately appears in Start Menu and Search.
 */
void startmenu_register_app(const char* name, const char* tag, uint32_t icon_color, void (*launch_fn)(void));

/*
 * startmenu_render:
 * Renders the Start Menu popup into the provided 32-bit framebuffer backbuffer.
 */
void startmenu_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme);

#endif /* GUI_STARTMENU_H */
