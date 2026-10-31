#ifndef GUI_LOCKSCREEN_H
#define GUI_LOCKSCREEN_H

#include "lib/types.h"

/*
 * lockscreen.h:
 * Hyprlock Minimalist Modal Lock Screen & Authentication System for MyOS.
 *
 * Why this is needed:
 * Implements a modern lock screen inspired by Hyprlock:
 * - Dims the desktop background with bitwise 50% alpha blending.
 * - Displays a centered card with a circular user monogram badge [ PV ].
 * - Displays a 2x-scaled typographic clock and date.
 * - Provides masked PIN input with red-shake animation on failed entry.
 * - Protects memory hygiene by zeroing PIN buffers immediately upon unlock.
 */

/* Initializes lock screen subsystem */
void lockscreen_init(void);

/* Activates the lock screen modal overlay */
void lockscreen_activate(void);

/* Checks if the lock screen is currently engaged */
bool lockscreen_is_active(void);

/* Renders the full lock screen overlay onto the back buffer */
void lockscreen_render(void);

/*
 * Dispatches keyboard strokes to the lock screen PIN buffer.
 * Returns true if the key unlocked the screen.
 */
bool lockscreen_handle_key(char c);

#endif /* GUI_LOCKSCREEN_H */
