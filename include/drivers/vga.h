#ifndef DRIVERS_VGA_H
#define DRIVERS_VGA_H

#include "lib/types.h"

/*
 * vga.h:
 * VGA 80x25 Text Mode Driver.
 *
 * Why this is needed:
 * Standard x86 PCs boot with VGA text mode 3 enabled. Memory at physical address
 * 0xB8000 maps directly to the video display buffer. Writing character and attribute
 * bytes to this buffer displays text on screen without requiring graphical rendering.
 */

#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  ((uint16_t*)0xB8000)

/* VGA Controller I/O Ports */
#define VGA_CRTC_INDEX_PORT  0x3D4  /* CRT Controller Index Register */
#define VGA_CRTC_DATA_PORT   0x3D5  /* CRT Controller Data Register */

/* VGA CRTC Cursor Position Registers */
#define VGA_CRTC_CURSOR_HIGH 0x0E   /* Cursor location high byte register */
#define VGA_CRTC_CURSOR_LOW  0x0F   /* Cursor location low byte register */

/* Standard 16-color VGA color palette */
typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
} vga_color_t;

/* Initializes VGA text mode driver, clears the screen, and homes the cursor */
void vga_init(void);

/* Clears the entire 80x25 screen using the current background color */
void vga_clear(void);

/* Sets the foreground and background text color attributes */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/* Prints a single character handling newlines, tabs, backspaces, and scrolling */
void vga_putchar(char c);

/* Prints a counted buffer of characters */
void vga_write(const char* data, size_t size);

/* Prints a null-terminated string to the VGA display */
void vga_writestring(const char* str);

/* Updates the hardware CRT blinking cursor location to match current row and column */
void vga_update_cursor(void);

/* Enables hardware blinking cursor and specifies scanlines */
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end);

/* Disables hardware blinking cursor */
void vga_disable_cursor(void);

/*
 * vga_put_at:
 * Writes a string directly at coordinates (x, y) with the specified color attribute byte.
 * Does NOT move or modify the hardware cursor or active text cursor position.
 */
void vga_put_at(size_t x, size_t y, const char* str, uint8_t color);

#endif /* DRIVERS_VGA_H */
