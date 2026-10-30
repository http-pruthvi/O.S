#ifndef GFX_CURSOR_H
#define GFX_CURSOR_H

#include "lib/types.h"

/*
 * cursor.h:
 * Software Mouse Cursor Engine with Save-Under Buffer.
 *
 * Why this is needed:
 * Standard PC hardware linear framebuffers do not provide a hardware mouse sprite.
 * This module manages a 12x19 software arrow cursor directly over video memory.
 * To ensure tear-free and flicker-free rendering:
 * 1. The off-screen back buffer NEVER contains the cursor.
 * 2. Background pixels are saved to a local save-under RAM buffer before drawing.
 * 3. Moving the cursor restores the old 12x19 area and draws the new 12x19 area,
 *    never triggering costly full-screen flushes.
 * 4. Dirty back buffer flushes overlapping the cursor temporarily hide and restore it.
 */

#define CURSOR_WIDTH  12
#define CURSOR_HEIGHT 19

/* Initializes the cursor position and save-under buffer */
void cursor_init(void);

/* Shows the cursor on screen (saves underlying pixels and draws sprite) */
void cursor_show(void);

/* Hides the cursor (restores saved background pixels to VRAM) */
void cursor_hide(void);

/* Moves cursor to (new_x, new_y), updating only the two 12x19 rectangles */
void cursor_move(int new_x, int new_y);

/* Returns whether the cursor is currently visible */
bool cursor_is_visible(void);

/* Returns current cursor coordinates */
void cursor_get_pos(int* x, int* y);

/* Checks if a dirty rectangle [x, y, w, h] overlaps the cursor bounding box */
bool cursor_overlaps_rect(int x, int y, int w, int h);

#endif /* GFX_CURSOR_H */
