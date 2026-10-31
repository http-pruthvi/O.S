#ifndef GUI_DOCK_H
#define GUI_DOCK_H

#include "lib/types.h"

/*
 * dock.h:
 * Floating Pill Application Dock for MyOS Desktop.
 *
 * Why this is needed:
 * Implements the modern floating application dock inspired by macOS / Hyprland:
 * - Centered at screen bottom with rounded aesthetic borders.
 * - Houses quick-launch icons for all core desktop apps and games.
 * - Shows active window indicators beneath running applications.
 */

#define DOCK_WIDTH   620
#define DOCK_HEIGHT  54
#define DOCK_X       ((1024 - DOCK_WIDTH) / 2)
#define DOCK_Y       706

/* Initializes floating dock subsystem */
void dock_init(void);

/* Renders the floating dock clipped to specified rectangle */
void dock_render(int clip_x, int clip_y, int clip_w, int clip_h);

/*
 * Handles mouse clicks on the floating dock.
 * Returns true if click was consumed.
 */
bool dock_handle_mouse(int mx, int my, bool left_pressed);

#endif /* GUI_DOCK_H */
