#include "gfx/fb_console.h"
#include "gfx/fb.h"
#include "gfx/font.h"
#include "lib/string.h"

/*
 * Standard 16-color VGA palette mapped to 32-bit 0x00RRGGBB RGB values.
 */
static const uint32_t vga_rgb_palette[16] = {
    0x00000000, /* 0: Black */
    0x000000AA, /* 1: Blue */
    0x0000AA00, /* 2: Green */
    0x0000AAAA, /* 3: Cyan */
    0x00AA0000, /* 4: Red */
    0x00AA00AA, /* 5: Magenta */
    0x00AA5500, /* 6: Brown */
    0x00AAAAAA, /* 7: Light Grey */
    0x00555555, /* 8: Dark Grey */
    0x005555FF, /* 9: Light Blue */
    0x0055FF55, /* 10: Light Green */
    0x0055FFFF, /* 11: Light Cyan */
    0x00FF5555, /* 12: Light Red */
    0x00FF55FF, /* 13: Light Magenta */
    0x00FFFF55, /* 14: Yellow */
    0x00FFFFFF  /* 15: White */
};

/* In-RAM character cell grid buffer (128 columns x 48 rows = 6144 cells, 12 KB in .bss) */
static fb_cell_t console_cells[FB_CONSOLE_ROWS][FB_CONSOLE_COLS];

static size_t  cursor_col = 0;
static size_t  cursor_row = 0;
static uint8_t current_color = 0x07; /* Light Grey on Black */

/*
 * fb_console_vga_to_rgb:
 * Converts a 4-bit standard VGA color index to a 32-bit RGB value.
 */
uint32_t fb_console_vga_to_rgb(uint8_t vga_color) {
    return vga_rgb_palette[vga_color & 0x0F];
}

/*
 * render_glyph:
 * Draws an 8x16 font character to VRAM at character grid coordinates (col, row).
 */
static void render_glyph(size_t col, size_t row, char c, uint8_t color_attr) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->is_available || !fb->virt_addr) {
        return;
    }

    if (col >= FB_CONSOLE_COLS || row >= FB_CONSOLE_ROWS) {
        return;
    }

    const uint8_t* glyph = font_get_glyph(c);
    uint32_t fg_rgb = fb_console_vga_to_rgb(color_attr & 0x0F);
    uint32_t bg_rgb = fb_console_vga_to_rgb((color_attr >> 4) & 0x0F);

    uint32_t base_x = (uint32_t)(col * FONT_GLYPH_WIDTH);
    uint32_t base_y = (uint32_t)(row * FONT_GLYPH_HEIGHT);

    for (uint32_t y = 0; y < FONT_GLYPH_HEIGHT; y++) {
        uint8_t bits = glyph[y];
        /* Calculate scanline start pointer in 32-bit VRAM */
        uint32_t* line = (uint32_t*)(fb->virt_addr + ((base_y + y) * fb->pitch) + (base_x * 4));

        line[0] = (bits & 0x80) ? fg_rgb : bg_rgb;
        line[1] = (bits & 0x40) ? fg_rgb : bg_rgb;
        line[2] = (bits & 0x20) ? fg_rgb : bg_rgb;
        line[3] = (bits & 0x10) ? fg_rgb : bg_rgb;
        line[4] = (bits & 0x08) ? fg_rgb : bg_rgb;
        line[5] = (bits & 0x04) ? fg_rgb : bg_rgb;
        line[6] = (bits & 0x02) ? fg_rgb : bg_rgb;
        line[7] = (bits & 0x01) ? fg_rgb : bg_rgb;
    }
}

/*
 * fb_console_repaint:
 * Repaints the entire display directly from the in-RAM cell array.
 * Note: Scrolling reads strictly from this RAM array, NEVER from VRAM,
 * avoiding slow PCI MMIO bus read penalties.
 */
void fb_console_repaint(void) {
    for (size_t r = 0; r < FB_CONSOLE_ROWS; r++) {
        for (size_t c = 0; c < FB_CONSOLE_COLS; c++) {
            render_glyph(c, r, console_cells[r][c].c, console_cells[r][c].color);
        }
    }
}

/*
 * scroll_console:
 * Shifts rows 1..47 up to 0..46 in the cell array, clears row 47,
 * and repaints the display from RAM.
 */
static void scroll_console(void) {
    /* 1. Shift lines up in fast system RAM */
    memmove(&console_cells[0][0],
            &console_cells[1][0],
            sizeof(fb_cell_t) * FB_CONSOLE_COLS * (FB_CONSOLE_ROWS - 1));

    /* 2. Clear bottom line in RAM */
    for (size_t c = 0; c < FB_CONSOLE_COLS; c++) {
        console_cells[FB_CONSOLE_ROWS - 1][c].c     = ' ';
        console_cells[FB_CONSOLE_ROWS - 1][c].color = current_color;
    }

    cursor_row = FB_CONSOLE_ROWS - 1;

    /* 3. Repaint from RAM to VRAM with zero VRAM reads */
    fb_console_repaint();
}

/*
 * fb_console_init:
 * Initializes the cell grid buffer and clears the screen.
 */
void fb_console_init(void) {
    cursor_col = 0;
    cursor_row = 0;
    current_color = 0x07; /* Light Grey on Black */
    fb_console_clear();
}

/*
 * fb_console_clear:
 * Clears both the in-RAM cell grid and the VRAM display.
 */
void fb_console_clear(void) {
    const fb_info_t* fb = fb_get_info();
    if (fb && fb->is_available && fb->virt_addr) {
        uint32_t bg_rgb = fb_console_vga_to_rgb((current_color >> 4) & 0x0F);
        size_t total_pixels = (fb->pitch / 4) * fb->height;
        uint32_t* vram = (uint32_t*)fb->virt_addr;
        for (size_t i = 0; i < total_pixels; i++) {
            vram[i] = bg_rgb;
        }
    }

    for (size_t r = 0; r < FB_CONSOLE_ROWS; r++) {
        for (size_t c = 0; c < FB_CONSOLE_COLS; c++) {
            console_cells[r][c].c     = ' ';
            console_cells[r][c].color = current_color;
        }
    }

    cursor_col = 0;
    cursor_row = 0;
}

/*
 * fb_console_putchar:
 * Writes a character to the current cursor position, advancing column/row,
 * and handles newlines, carriage returns, tabs, backspaces, and scrolling.
 */
void fb_console_putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        /* Advance to next 8-character tab stop */
        cursor_col = (cursor_col + 8) & ~7;
        if (cursor_col >= FB_CONSOLE_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    } else if (c == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            console_cells[cursor_row][cursor_col].c = ' ';
            console_cells[cursor_row][cursor_col].color = current_color;
            render_glyph(cursor_col, cursor_row, ' ', current_color);
        }
    } else {
        /* Printable character */
        if (cursor_col >= FB_CONSOLE_COLS) {
            cursor_col = 0;
            cursor_row++;
        }

        if (cursor_row >= FB_CONSOLE_ROWS) {
            scroll_console();
        }

        console_cells[cursor_row][cursor_col].c = c;
        console_cells[cursor_row][cursor_col].color = current_color;
        render_glyph(cursor_col, cursor_row, c, current_color);
        cursor_col++;
    }

    if (cursor_row >= FB_CONSOLE_ROWS) {
        scroll_console();
    }
}

/*
 * fb_console_write:
 * Outputs a null-terminated string to the framebuffer terminal.
 */
void fb_console_write(const char* str) {
    if (!str) return;
    while (*str) {
        fb_console_putchar(*str++);
    }
}

/*
 * fb_console_put_at:
 * Writes a string at a fixed character cell location without moving the main cursor.
 * Updates both the in-RAM cell array and the video display.
 */
void fb_console_put_at(size_t col, size_t row, const char* str, uint8_t color) {
    if (!str || row >= FB_CONSOLE_ROWS) {
        return;
    }

    size_t cur_c = col;
    while (*str && cur_c < FB_CONSOLE_COLS) {
        char ch = *str++;
        console_cells[row][cur_c].c = ch;
        console_cells[row][cur_c].color = color;
        render_glyph(cur_c, row, ch, color);
        cur_c++;
    }
}

/*
 * fb_console_set_color:
 * Updates the active foreground and background color attributes.
 */
void fb_console_set_color(vga_color_t fg, vga_color_t bg) {
    current_color = (uint8_t)(((bg & 0x0F) << 4) | (fg & 0x0F));
}
