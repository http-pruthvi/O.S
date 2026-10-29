#include "drivers/vga.h"
#include "gfx/fb.h"
#include "gfx/fb_console.h"
#include "lib/io.h"
#include "lib/string.h"

/* Current cursor row (0 to 24) and column (0 to 79) */
static size_t vga_row = 0;
static size_t vga_column = 0;

/* Current color attribute byte (high nibble: background, low nibble: foreground) */
static uint8_t vga_color = 0;

/* Pointer to VGA text mode framebuffer at physical address 0xB8000 */
static uint16_t* const vga_buffer = VGA_MEMORY;

/*
 * vga_entry_color:
 * Combines foreground and background 4-bit color codes into a single 8-bit VGA attribute.
 */
static inline uint8_t vga_entry_color(vga_color_t fg, vga_color_t bg) {
    return (uint8_t)(fg | (bg << 4));
}

/*
 * vga_entry:
 * Packs an ASCII character and an attribute byte into a 16-bit VGA cell entry.
 * VGA hardware interprets the lower 8 bits as character code and upper 8 bits as attribute.
 */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | ((uint16_t)color << 8);
}

/*
 * vga_enable_cursor:
 * Enables the VGA hardware blinking cursor and configures its scanline bounds.
 *
 * Parameters:
 *   cursor_start - Starting scanline within character cell (0-15)
 *   cursor_end   - Ending scanline within character cell (0-15)
 */
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
    /* Select Cursor Start Register (Index 0x0A) */
    outb(VGA_CRTC_INDEX_PORT, 0x0A);
    /* Bit 5 of register 0x0A is Cursor Disable bit (0 = enabled).
     * Bits 0-4 specify the starting scanline. */
    outb(VGA_CRTC_DATA_PORT, (inb(VGA_CRTC_DATA_PORT) & 0xC0) | cursor_start);

    /* Select Cursor End Register (Index 0x0B) */
    outb(VGA_CRTC_INDEX_PORT, 0x0B);
    /* Bits 0-4 specify the ending scanline. */
    outb(VGA_CRTC_DATA_PORT, (inb(VGA_CRTC_DATA_PORT) & 0xE0) | cursor_end);
}

/*
 * vga_disable_cursor:
 * Disables the hardware blinking cursor by setting bit 5 of CRTC register 0x0A.
 */
void vga_disable_cursor(void) {
    outb(VGA_CRTC_INDEX_PORT, 0x0A);
    outb(VGA_CRTC_DATA_PORT, 0x20); /* Bit 5 = 1 disables cursor */
}

/*
 * vga_update_cursor:
 * Updates the hardware text cursor position to match vga_row and vga_column.
 *
 * Why this is needed:
 * While software writes characters to RAM at 0xB8000, the blinking visual cursor
 * is generated independently by the VGA CRT Controller chip, which must be told
 * the linear character offset (row * 80 + col) via I/O ports 0x3D4 and 0x3D5.
 */
void vga_update_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_WIDTH + vga_column);

    /* Send high byte of cursor position to CRTC register 0x0E */
    outb(VGA_CRTC_INDEX_PORT, VGA_CRTC_CURSOR_HIGH);
    outb(VGA_CRTC_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));

    /* Send low byte of cursor position to CRTC register 0x0F */
    outb(VGA_CRTC_INDEX_PORT, VGA_CRTC_CURSOR_LOW);
    outb(VGA_CRTC_DATA_PORT, (uint8_t)(pos & 0xFF));
}

/*
 * vga_set_color:
 * Updates the active foreground and background colors for subsequent output.
 *
 * Parameters:
 *   fg - Foreground color
 *   bg - Background color
 */
void vga_set_color(vga_color_t fg, vga_color_t bg) {
    vga_color = vga_entry_color(fg, bg);
    if (fb_is_active()) {
        fb_console_set_color(fg, bg);
    }
}

/*
 * vga_scroll:
 * Scrolls the screen up by one row when printing reaches beyond the bottom (row 25).
 *
 * Why this is needed:
 * Without scrolling, characters written past row 24 would overflow beyond the 80x25
 * display buffer into unrelated video RAM pages.
 */
static void vga_scroll(void) {
    /* Copy rows 1..24 up by one row to rows 0..23 */
    for (size_t y = 1; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] = vga_buffer[y * VGA_WIDTH + x];
        }
    }

    /* Blank out the bottom row (row 24) with spaces in the current color */
    uint16_t blank = vga_entry(' ', vga_color);
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = blank;
    }

    /* Move cursor back to the start of the bottom row */
    vga_row = VGA_HEIGHT - 1;
    vga_column = 0;
}

/*
 * vga_clear:
 * Clears the entire text screen by writing blank spaces with the active color.
 * Routes to fb_console_clear() when in graphics mode.
 */
void vga_clear(void) {
    if (fb_is_active()) {
        fb_console_clear();
        return;
    }

    uint16_t blank = vga_entry(' ', vga_color);
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = blank;
        }
    }
    vga_row = 0;
    vga_column = 0;
    vga_update_cursor();
}

/*
 * vga_init:
 * Initializes the VGA driver with light grey text on black background,
 * clears the screen, and homes the cursor.
 */
void vga_init(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_enable_cursor(14, 15); /* Lower 2 scanlines give standard underline cursor */
    vga_clear();
}

/*
 * vga_putchar:
 * Writes a single ASCII character to the active display output.
 * If linear graphics mode is active, routes directly to the framebuffer terminal console.
 * Otherwise, outputs to the legacy 80x25 VGA text buffer at 0xB8000.
 *
 * Parameters:
 *   c - Character to output
 */
void vga_putchar(char c) {
    if (fb_is_active()) {
        fb_console_putchar(c);
        return;
    }

    if (c == '\n') {
        /* Line feed: move to beginning of next row */
        vga_column = 0;
        vga_row++;
    } else if (c == '\r') {
        /* Carriage return: move to beginning of current row */
        vga_column = 0;
    } else if (c == '\t') {
        /* Tab: advance to the next 4-space tab stop */
        vga_column = (vga_column + 4) & ~3;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
        }
    } else if (c == '\b') {
        /* Backspace: move cursor backward and erase previous character */
        if (vga_column > 0) {
            vga_column--;
            size_t index = vga_row * VGA_WIDTH + vga_column;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    } else {
        /* Printable character: write to current cell */
        size_t index = vga_row * VGA_WIDTH + vga_column;
        vga_buffer[index] = vga_entry((unsigned char)c, vga_color);
        vga_column++;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
        }
    }

    /* Scroll screen if we have moved past the bottom row */
    if (vga_row >= VGA_HEIGHT) {
        vga_scroll();
    }

    vga_update_cursor();
}

/*
 * vga_write:
 * Outputs a buffer of characters of specified length.
 *
 * Parameters:
 *   data - Pointer to character buffer
 *   size - Number of bytes to print
 */
void vga_write(const char* data, size_t size) {
    if (!data) return;
    for (size_t i = 0; i < size; i++) {
        vga_putchar(data[i]);
    }
}

/*
 * vga_writestring:
 * Outputs a null-terminated string to the VGA display.
 *
 * Parameters:
 *   str - Pointer to null-terminated ASCII string
 */
void vga_writestring(const char* str) {
    if (!str) return;
    vga_write(str, strlen(str));
}

/*
 * vga_put_at:
 * Writes a null-terminated string directly to the VGA text buffer starting at (x, y)
 * without altering the active text cursor coordinates (vga_row, vga_column) or
 * moving the hardware blinking cursor.
 *
 * Parameters:
 *   x     - Horizontal column coordinate (0 to VGA_WIDTH - 1)
 *   y     - Vertical row coordinate (0 to VGA_HEIGHT - 1)
 *   str   - Null-terminated string to display
 *   color - VGA attribute byte (foreground and background color)
 */
void vga_put_at(size_t x, size_t y, const char* str, uint8_t color) {
    if (!str) {
        return;
    }

    if (fb_is_active()) {
        fb_console_put_at(x, y, str, color);
        return;
    }

    if (y >= VGA_HEIGHT) {
        return;
    }

    size_t col = x;
    for (size_t i = 0; str[i] != '\0' && col < VGA_WIDTH; i++) {
        size_t index = y * VGA_WIDTH + col;
        vga_buffer[index] = vga_entry((unsigned char)str[i], color);
        col++;
    }
}

