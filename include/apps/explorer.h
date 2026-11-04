#ifndef APPS_EXPLORER_H
#define APPS_EXPLORER_H

/*
 * explorer.h:
 * Graphical File Explorer Application.
 *
 * Why this is needed:
 * Provides a desktop GUI file manager for browsing directories, viewing file
 * sizes and types, opening documents inside the Text Editor, creating new files,
 * and deleting files from the filesystem.
 */

#include "lib/types.h"

/* Opens the graphical File Explorer window or brings it to the foreground */
void app_explorer_open(void);

/* Checks if File Explorer context menu is currently open */
bool explorer_context_menu_is_open(void);

/* Composites modern Windows 11 Fluent context menu onto screen back buffer */
void explorer_context_menu_render(uint32_t* backbuf, int screen_w, int screen_h);

/* Dispatches mouse click to context menu (returns true if handled or consumed) */
bool explorer_context_menu_handle_click(int mx, int my, bool is_right_click);

/* Dismisses active context menu */
void explorer_context_menu_dismiss(void);

#endif /* APPS_EXPLORER_H */

