#ifndef APPS_BREAKOUT_H
#define APPS_BREAKOUT_H

#include "lib/types.h"
#include "gui/window.h"

/*
 * breakout.h:
 * Classic Retro 8-Bit Breakout / Brick Breaker Arcade Game for MyOS.
 *
 * Why this is needed:
 * Implements an authentic retro arcade desktop game demonstrating:
 * 1. Sub-pixel fixed-point physics and bounce reflection algorithms.
 * 2. Segmented paddle deflection physics where rebound angles vary by hit position.
 * 3. 4-tier colored brick grid destruction with score tracking.
 * 4. Multi-state lifecycle (Ready, Playing, Paused, Game Over, Victory).
 * 5. Real-time dual keyboard (Arrows/WASD) and smooth mouse paddle tracking.
 */

/*
 * breakout_app_create:
 * Creates and registers the Breakout window at the specified screen coordinates.
 * Returns pointer to the newly created window_t or existing window.
 */
window_t* breakout_app_create(int32_t x, int32_t y);

/*
 * breakout_tick:
 * Periodically called from the desktop main event loop to update ball movement,
 * detect collisions, process mouse paddle tracking, and refresh dirty areas.
 */
void breakout_tick(void);

/*
 * breakout_handle_key:
 * Dispatches keypress events (both raw hardware scancodes and ASCII characters)
 * to steer the paddle, serve the ball, pause, or restart after game over.
 */
void breakout_handle_key(uint8_t scancode);

/*
 * app_breakout_open:
 * Standard desktop launcher hook to open or focus the Breakout application.
 */
void app_breakout_open(void);

#endif /* APPS_BREAKOUT_H */
