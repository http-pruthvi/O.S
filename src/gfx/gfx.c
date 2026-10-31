#include "gfx/gfx.h"
#include "gfx/font.h"
#include "mm/heap.h"
#include "mm/pmm.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Pointer to the off-screen 32-bit pixel back buffer allocated in system RAM */
static uint32_t* back_buffer = NULL;

/*
 * Weak cursor hooks:
 * These weak stubs allow gfx.o to link independently before the cursor module
 * is introduced in Commit 9. When cursor.o provides strong definitions, the linker
 * automatically binds to them.
 */
__attribute__((weak)) bool cursor_overlaps_rect(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    return false;
}

__attribute__((weak)) bool cursor_is_visible(void) {
    return false;
}

__attribute__((weak)) void cursor_hide(void) {}

__attribute__((weak)) void cursor_show(void) {}

/*
 * abs_val:
 * Helper for integer absolute value.
 */
static inline int abs_val(int n) {
    return (n < 0) ? -n : n;
}

/*
 * gfx_init:
 * Allocates the off-screen back buffer (~3 MB for 1024x768x32).
 * Verifies heap break and physical memory frames before and after.
 * Gracefully returns false and stays in console mode if memory is exhausted.
 */
bool gfx_init(void) {
    if (back_buffer) {
        return true; /* Already initialized */
    }

    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->is_available || !fb->virt_addr) {
        serial_writestring("[GFX] Framebuffer not available. Cannot initialize 2D graphics.\n");
        return false;
    }

    size_t buffer_size = fb->pitch * fb->height;

    uintptr_t break_before = heap_get_break();
    uint32_t pmm_free_before = pmm_free_frames();

    serial_writestring("[GFX] Allocating 3 MB back buffer from kernel heap...\n");
    kprintf("[GFX] Allocating %u KB back buffer... (Heap break: %p, PMM free: %u frames)\n",
            (uint32_t)(buffer_size / 1024), (void*)break_before, pmm_free_before);

    back_buffer = (uint32_t*)kmalloc(buffer_size);
    if (!back_buffer) {
        serial_writestring("[GFX] ERROR: Failed to allocate back buffer. Staying in framebuffer console.\n");
        kprintf("[GFX] ERROR: kmalloc(%u bytes) returned NULL! Staying in framebuffer console.\n",
                (uint32_t)buffer_size);
        return false;
    }

    uintptr_t break_after = heap_get_break();
    uint32_t pmm_free_after = pmm_free_frames();

    serial_writestring("[GFX] Back buffer allocated successfully.\n");
    kprintf("[GFX] Back buffer allocated at %p\n", (void*)back_buffer);
    kprintf("      Heap break: %p -> %p (expanded by %u KB)\n",
            (void*)break_before, (void*)break_after,
            (uint32_t)((break_after - break_before) / 1024));
    kprintf("      PMM free:   %u -> %u frames (used %u frames)\n",
            pmm_free_before, pmm_free_after, pmm_free_before - pmm_free_after);

    /* Initialize back buffer to solid black */
    gfx_clear(GFX_COLOR_BLACK);
    return true;
}

/*
 * gfx_is_ready:
 * Checks whether the back buffer has been allocated and is ready for drawing.
 */
bool gfx_is_ready(void) {
    return (back_buffer != NULL);
}

/*
 * gfx_get_backbuffer:
 * Returns pointer to the raw pixel buffer.
 */
uint32_t* gfx_get_backbuffer(void) {
    return back_buffer;
}

/*
 * gfx_putpixel:
 * Plots a single 32-bit pixel into the back buffer at (x, y) with bounds checking.
 */
void gfx_putpixel(int x, int y, uint32_t color) {
    if (!back_buffer) return;

    const fb_info_t* fb = fb_get_info();
    if (!fb || x < 0 || y < 0 || (uint32_t)x >= fb->width || (uint32_t)y >= fb->height) {
        return;
    }

    uint32_t pitch_dwords = fb->pitch / 4;
    back_buffer[(uint32_t)y * pitch_dwords + (uint32_t)x] = color;
}

/*
 * gfx_getpixel:
 * Reads a 32-bit pixel from the back buffer at (x, y).
 */
uint32_t gfx_getpixel(int x, int y) {
    if (!back_buffer) return GFX_COLOR_BLACK;

    const fb_info_t* fb = fb_get_info();
    if (!fb || x < 0 || y < 0 || (uint32_t)x >= fb->width || (uint32_t)y >= fb->height) {
        return GFX_COLOR_BLACK;
    }

    uint32_t pitch_dwords = fb->pitch / 4;
    return back_buffer[(uint32_t)y * pitch_dwords + (uint32_t)x];
}

/*
 * gfx_fill_rect:
 * Draws a solid filled rectangle into the back buffer with screen boundary clipping.
 */
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color) {
    if (!back_buffer || width <= 0 || height <= 0) return;

    const fb_info_t* fb = fb_get_info();
    if (!fb) return;

    /* Clip rectangle to screen boundaries */
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > (int)fb->width) {
        width = (int)fb->width - x;
    }
    if (y + height > (int)fb->height) {
        height = (int)fb->height - y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    uint32_t pitch_dwords = fb->pitch / 4;
    for (int r = y; r < y + height; r++) {
        uint32_t* line = &back_buffer[(uint32_t)r * pitch_dwords + (uint32_t)x];
        for (int c = 0; c < width; c++) {
            line[c] = color;
        }
    }
}

/*
 * gfx_draw_rect:
 * Draws an un-filled outline rectangle of 1-pixel thickness into the back buffer.
 */
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color) {
    if (width <= 0 || height <= 0) return;

    /* Top and bottom horizontal borders */
    gfx_fill_rect(x, y, width, 1, color);
    gfx_fill_rect(x, y + height - 1, width, 1, color);

    /* Left and right vertical borders */
    gfx_fill_rect(x, y, 1, height, color);
    gfx_fill_rect(x + width - 1, y, 1, height, color);
}

/*
 * gfx_draw_line:
 * Draws a 1-pixel line from (x0, y0) to (x1, y1) using Bresenham's line algorithm.
 */
void gfx_draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs_val(x1 - x0);
    int dy = -abs_val(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        gfx_putpixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/*
 * gfx_draw_line_thick:
 * Draws a line with given thickness (diameter in pixels) by stamping filled squares
 * along the Bresenham line path. Guarantees no gaps during fast mouse movement.
 */
void gfx_draw_line_thick(int x0, int y0, int x1, int y1, int thickness, uint32_t color) {
    if (thickness <= 1) {
        gfx_draw_line(x0, y0, x1, y1, color);
        return;
    }

    int half = thickness / 2;
    int dx = abs_val(x1 - x0);
    int dy = -abs_val(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (1) {
        gfx_fill_rect(x0 - half, y0 - half, thickness, thickness, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

/*
 * gfx_draw_char:
 * Draws an 8x16 bitmap font glyph into the back buffer.
 */
void gfx_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, bool transparent_bg) {
    if (!back_buffer) return;

    const fb_info_t* fb = fb_get_info();
    if (!fb) return;

    const uint8_t* glyph = font_get_glyph(c);
    uint32_t pitch_dwords = fb->pitch / 4;

    for (uint32_t row = 0; row < FONT_GLYPH_HEIGHT; row++) {
        int py = y + (int)row;
        if (py < 0 || (uint32_t)py >= fb->height) {
            continue;
        }

        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_GLYPH_WIDTH; col++) {
            int px = x + (int)col;
            if (px < 0 || (uint32_t)px >= fb->width) {
                continue;
            }

            if (bits & (0x80 >> col)) {
                back_buffer[(uint32_t)py * pitch_dwords + (uint32_t)px] = fg_color;
            } else if (!transparent_bg) {
                back_buffer[(uint32_t)py * pitch_dwords + (uint32_t)px] = bg_color;
            }
        }
    }
}

/*
 * gfx_draw_string:
 * Renders a null-terminated text string into the back buffer.
 */
void gfx_draw_string(int x, int y, const char* str, uint32_t fg_color, uint32_t bg_color, bool transparent_bg) {
    if (!str) return;

    int cur_x = x;
    while (*str) {
        gfx_draw_char(cur_x, y, *str, fg_color, bg_color, transparent_bg);
        cur_x += FONT_GLYPH_WIDTH;
        str++;
    }
}

/*
 * gfx_draw_char_scaled:
 * Draws an 8x16 bitmap glyph scaled up by integer factor (e.g. scale=2 produces 16x32).
 */
void gfx_draw_char_scaled(int x, int y, char c, int scale, uint32_t fg_color, uint32_t bg_color, bool transparent_bg) {
    if (!back_buffer || scale <= 0) return;

    const uint8_t* glyph = font_get_glyph(c);
    if (!glyph) return;

    for (uint32_t row = 0; row < FONT_GLYPH_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint32_t col = 0; col < FONT_GLYPH_WIDTH; col++) {
            bool on = (bits & (0x80 >> col)) != 0;
            if (on) {
                gfx_fill_rect(x + (int)col * scale, y + (int)row * scale, scale, scale, fg_color);
            } else if (!transparent_bg) {
                gfx_fill_rect(x + (int)col * scale, y + (int)row * scale, scale, scale, bg_color);
            }
        }
    }
}

/*
 * gfx_draw_string_scaled:
 * Renders a null-terminated string using scaled bitmap font.
 */
void gfx_draw_string_scaled(int x, int y, const char* str, int scale, uint32_t fg_color, uint32_t bg_color, bool transparent_bg) {
    if (!str || scale <= 0) return;
    int cur_x = x;
    while (*str) {
        gfx_draw_char_scaled(cur_x, y, *str, scale, fg_color, bg_color, transparent_bg);
        cur_x += FONT_GLYPH_WIDTH * scale;
        str++;
    }
}

/*
 * gfx_clear:
 * Fills the entire back buffer with a solid 32-bit color.
 */
void gfx_clear(uint32_t color) {
    if (!back_buffer) return;

    const fb_info_t* fb = fb_get_info();
    if (!fb) return;

    size_t total_pixels = (fb->pitch / 4) * fb->height;
    for (size_t i = 0; i < total_pixels; i++) {
        back_buffer[i] = color;
    }
}

/*
 * gfx_flush_rect:
 * Copies a dirty rectangular region from the off-screen back buffer to VRAM.
 *
 * Rules:
 * - The back buffer NEVER contains the cursor.
 * - If the dirty rectangle overlaps the active cursor, the cursor is hidden
 *   (restoring its old background), the back buffer is copied to VRAM, and then
 *   the cursor is restored (re-saving background from back buffer and redrawing).
 */
void gfx_flush_rect(int x, int y, int width, int height) {
    if (!back_buffer) return;

    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->virt_addr) return;

    /* Clip dirty rectangle to screen boundaries */
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > (int)fb->width) {
        width = (int)fb->width - x;
    }
    if (y + height > (int)fb->height) {
        height = (int)fb->height - y;
    }

    if (width <= 0 || height <= 0) {
        return;
    }

    /* Check cursor overlap */
    bool cursor_hidden = false;
    if (cursor_is_visible() && cursor_overlaps_rect(x, y, width, height)) {
        cursor_hide();
        cursor_hidden = true;
    }

    /* Row-by-row memory copy from RAM back buffer to video memory */
    uint32_t pitch_dwords = fb->pitch / 4;
    uint32_t* vram = (uint32_t*)fb->virt_addr;

    for (int r = y; r < y + height; r++) {
        uint32_t offset = (uint32_t)r * pitch_dwords + (uint32_t)x;
        memcpy(&vram[offset], &back_buffer[offset], (size_t)width * sizeof(uint32_t));
    }

    if (cursor_hidden) {
        cursor_show();
    }
}

/*
 * gfx_flush:
 * Copies the entire back buffer to video memory.
 */
void gfx_flush(void) {
    const fb_info_t* fb = fb_get_info();
    if (fb) {
        gfx_flush_rect(0, 0, (int)fb->width, (int)fb->height);
    }
}
