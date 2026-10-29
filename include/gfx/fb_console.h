#ifndef GFX_FB_CONSOLE_H
#define GFX_FB_CONSOLE_H

#include "lib/types.h"
#include "drivers/vga.h"

/*
 * fb_console.h:
 * 128x48 Text Terminal Emulator over Linear Framebuffer.
 *
 * Why this is needed:
 * In linear graphics mode, writing to legacy VGA text buffer at 0xB8000
 * produces no visible output. This module replaces the VGA text driver with
 * an in-memory 128x48 character cell grid, rendering 8x16 font glyphs directly
 * to video memory, with support for scrolling, colors, and direct cell positioning.
 */

#define FB_CONSOLE_COLS 128
#define FB_CONSOLE_ROWS 48

/* Character cell entry stored in system RAM */
typedef struct {
    char    c;
    uint8_t color; /* High nibble = background (0-15), Low nibble = foreground (0-15) */
} fb_cell_t;

/* Initializes the framebuffer terminal console */
void fb_console_init(void);

/* Writes a single character to the console at the current cursor location */
void fb_console_putchar(char c);

/* Writes a null-terminated string to the console */
void fb_console_write(const char* str);

/* Writes a string at a fixed row and column without moving the active cursor */
void fb_console_put_at(size_t col, size_t row, const char* str, uint8_t color);

/* Sets the active foreground and background color attributes (VGA color codes 0-15) */
void fb_console_set_color(vga_color_t fg, vga_color_t bg);

/* Clears the console and resets cursor to (0, 0) */
void fb_console_clear(void);

/* Repaints the entire display from the in-RAM cell array (used after GUI overlays) */
void fb_console_repaint(void);

/* Converts a 4-bit standard VGA color index (0-15) to a 32-bit RGB color (0x00RRGGBB) */
uint32_t fb_console_vga_to_rgb(uint8_t vga_color);

#endif /* GFX_FB_CONSOLE_H */
