#ifndef GUI_HUD_H
#define GUI_HUD_H

#include "lib/types.h"

/*
 * hud.h:
 * Conky-style Desktop HUD (Heads-Up Display) widgets for MyOS.
 *
 * Why this is needed:
 * Renders aesthetic floating telemetry cards directly on the desktop wallpaper
 * beneath application windows, providing:
 * 1. Large 2x-scaled typographic digital clock and date card.
 * 2. Real-time physical memory (PMM) and kernel heap usage gauges with meter bars.
 * 3. Kernel system metrics (Architecture, Scheduler, Active Task Count).
 */

/* Initializes HUD subsystem */
void hud_init(void);

/* Toggles HUD widget rendering on or off */
void hud_toggle(void);

/* Checks if HUD widgets are currently enabled */
bool hud_is_enabled(void);

/*
 * hud_render:
 * Renders the floating HUD cards clipped to the dirty rectangle.
 * Called by the window manager after wallpaper rendering and before windows.
 */
void hud_render(int clip_x, int clip_y, int clip_w, int clip_h);

#endif /* GUI_HUD_H */
