#ifndef GFX_GFX_H
#define GFX_GFX_H

#include "lib/types.h"
#include "gfx/fb.h"

/*
 * gfx.h:
 * 2D Graphics Primitives and Off-Screen Back Buffer Engine.
 *
 * Why this is needed:
 * Rendering complex graphics directly to video memory causes visible tearing,
 * flickering, and slow PCI MMIO bus overhead. The 2D graphics engine manages
 * a 32-bit off-screen back buffer in fast system RAM (~3 MB at 1024x768x32).
 * All primitives (lines, rectangles, glyphs) draw into the back buffer, and
 * only modified regions are flushed to VRAM via gfx_flush_rect.
 */

/* Standard 32-bit RGB Color Definitions (0x00RRGGBB) */
#define GFX_COLOR_BLACK       0x00000000
#define GFX_COLOR_WHITE       0x00FFFFFF
#define GFX_COLOR_RED         0x00E02020
#define GFX_COLOR_GREEN       0x0020B020
#define GFX_COLOR_BLUE        0x002050D0
#define GFX_COLOR_YELLOW      0x00F0D020
#define GFX_COLOR_CYAN        0x0020C0D0
#define GFX_COLOR_MAGENTA     0x00D020D0
#define GFX_COLOR_DARK_GREY   0x00303030
#define GFX_COLOR_MID_GREY    0x00808080
#define GFX_COLOR_LIGHT_GREY  0x00D0D0D0
#define GFX_COLOR_LIGHT_BLUE  0x004080F0
#define GFX_COLOR_DARK_BLUE   0x00102050
#define GFX_COLOR_ORANGE      0x00E07010
#define GFX_COLOR_BROWN       0x00804010

/* Initializes 2D graphics subsystem and allocates the back buffer via kmalloc */
bool gfx_init(void);

/* Returns true if graphics subsystem and back buffer are ready */
bool gfx_is_ready(void);

/* Returns pointer to back buffer pixel array */
uint32_t* gfx_get_backbuffer(void);

/* Plots a single pixel into the back buffer at (x, y) with clipping */
void gfx_putpixel(int x, int y, uint32_t color);

/* Reads a pixel value from the back buffer at (x, y) */
uint32_t gfx_getpixel(int x, int y);

/* Draws a 1-pixel line from (x0, y0) to (x1, y1) using Bresenham's algorithm */
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color);

/* Draws a thick line (diameter in pixels) using swept brush squares */
void gfx_draw_line_thick(int x0, int y0, int x1, int y1, int thickness, uint32_t color);

/* Draws an outline rectangle */
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);

/* Draws a filled rectangle with clipping */
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);

/* Draws a single character glyph using font8x16 */
void gfx_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, bool transparent_bg);

/* Draws a null-terminated string */
void gfx_draw_string(int x, int y, const char* str, uint32_t fg_color, uint32_t bg_color, bool transparent_bg);

/* Clears the entire back buffer to a solid color */
void gfx_clear(uint32_t color);

/* Flushes a dirty rectangular region from the back buffer to VRAM (handles cursor overlap) */
void gfx_flush_rect(int x, int y, int width, int height);

/* Flushes the entire back buffer to VRAM */
void gfx_flush(void);

#endif /* GFX_GFX_H */
