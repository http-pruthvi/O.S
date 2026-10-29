#ifndef GFX_FONT_H
#define GFX_FONT_H

#include "lib/types.h"

/*
 * font.h:
 * 8x16 Monochrome Bitmap Font Interface.
 *
 * Why this is needed:
 * In linear graphics mode (1024x768x32), the hardware VGA character generator ROM
 * is not accessible. Rendering text requires software rasterization of glyph bitmaps.
 * This module provides 8x16 bitmap glyph slices for standard printable ASCII characters
 * (32 ' ' through 126 '~').
 */

#define FONT_GLYPH_WIDTH   8
#define FONT_GLYPH_HEIGHT  16
#define FONT_FIRST_CHAR    32  /* ASCII space */
#define FONT_LAST_CHAR     126 /* ASCII tilde */
#define FONT_NUM_GLYPHS    (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1) /* 95 printable characters */

/*
 * font_get_glyph:
 * Returns pointer to the 16-byte bitmap array for character 'c'.
 * Each byte represents 1 horizontal scanline (bit 7 is leftmost pixel).
 * Characters outside the printable range [32, 126] return glyph for '?'.
 */
const uint8_t* font_get_glyph(char c);

#endif /* GFX_FONT_H */
