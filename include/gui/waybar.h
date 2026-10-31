#ifndef GUI_WAYBAR_H
#define GUI_WAYBAR_H

#include "lib/types.h"

/*
 * waybar.h:
 * Top Waybar-style Status Bar for MyOS Desktop.
 *
 * Why this is needed:
 * Implements the modern top status bar inspired by Waybar / Polybar in Hyprland ricing:
 * - Persistent workspace indicators ([ 1 ] [ 2 ] [ 3 ]).
 * - Centered system brand status chip.
 * - Hardware telemetry chips (RAM % meter, Task count).
 * - Interactive Lock Screen trigger button.
 * - Live digital clock chip.
 */

#define WAYBAR_HEIGHT 28

/* Initializes Waybar top bar subsystem */
void waybar_init(void);

/* Renders the top status bar clipped to the specified rectangle */
void waybar_render(int clip_x, int clip_y, int clip_w, int clip_h);

/*
 * Handles mouse clicks on Waybar (workspace pills, lock button).
 * Returns true if the click was consumed.
 */
bool waybar_handle_mouse(int mx, int my, bool left_pressed);

/* Returns current active workspace (1, 2, or 3) */
int waybar_get_active_workspace(void);

/* Switches active workspace */
void waybar_set_active_workspace(int ws);

#endif /* GUI_WAYBAR_H */
