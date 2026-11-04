#include "gfx/cursor.h"
#include "gfx/fb.h"
#include "gfx/gfx.h"

/*
 * 12x19 Arrow Cursor Bitmap:
 * 0 = Transparent
 * 1 = Black Outline (0x00000000)
 * 2 = White Interior (0x00FFFFFF)
 */
static const uint8_t cursor_bitmap[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* Row 0:  *            */
    { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* Row 1:  **           */
    { 1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* Row 2:  *.*          */
    { 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0 }, /* Row 3:  *..*         */
    { 1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0 }, /* Row 4:  *...*        */
    { 1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0 }, /* Row 5:  *....*       */
    { 1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0 }, /* Row 6:  *.....*      */
    { 1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0 }, /* Row 7:  *......*     */
    { 1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0 }, /* Row 8:  *.......*    */
    { 1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0 }, /* Row 9:  *........*   */
    { 1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0 }, /* Row 10: *......*****  */
    { 1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0 }, /* Row 11: *..*..*      */
    { 1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0 }, /* Row 12: *.* *..*     */
    { 1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 }, /* Row 13: **   *..*    */
    { 1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0 }, /* Row 14: *     *..*   */
    { 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0 }, /* Row 15:       *..*   */
    { 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0 }, /* Row 16:        *..*  */
    { 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0 }, /* Row 17:        *..*  */
    { 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0 }  /* Row 18:         **   */
};

/* Horizontal Resize (<->) Cursor Bitmap */
static const uint8_t cursor_bitmap_resize_h[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
    { 0, 1, 2, 1, 0, 0, 0, 0, 1, 2, 1, 0 },
    { 1, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 1 },
    { 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1 },
    { 1, 2, 2, 2, 1, 1, 1, 1, 2, 2, 2, 1 },
    { 0, 1, 2, 1, 0, 0, 0, 0, 1, 2, 1, 0 },
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Vertical Resize (^v) Cursor Bitmap */
static const uint8_t cursor_bitmap_resize_v[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 1, 2, 2, 2, 2, 1, 0, 0, 0 },
    { 0, 0, 1, 2, 2, 2, 2, 2, 2, 1, 0, 0 },
    { 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 0 },
    { 0, 0, 1, 2, 2, 2, 2, 2, 2, 1, 0, 0 },
    { 0, 0, 0, 1, 2, 2, 2, 2, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Diagonal Bottom-Right Resize (\) Cursor Bitmap */
static const uint8_t cursor_bitmap_resize_diag_br[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 2, 1, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 0, 1, 2, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 2, 1, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 0 },
    { 0, 0, 0, 0, 0, 1, 2, 1, 2, 1, 1, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 2, 2, 2, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Diagonal Bottom-Left Resize (/) Cursor Bitmap */
static const uint8_t cursor_bitmap_resize_diag_bl[CURSOR_HEIGHT][CURSOR_WIDTH] = {
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 1, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0 },
    { 0, 0, 0, 0, 0, 0, 1, 2, 1, 2, 1, 0 },
    { 0, 0, 0, 0, 0, 1, 2, 1, 0, 1, 1, 0 },
    { 0, 0, 0, 0, 1, 2, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 0, 1, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 1, 1, 2, 1, 2, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 2, 2, 1, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 2, 2, 2, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

/* Save-under RAM buffer holding the pixels obscured by the cursor */
static uint32_t save_under[CURSOR_HEIGHT][CURSOR_WIDTH];

/* Cursor state */
static int           cursor_x = 512;
static int           cursor_y = 384;
static bool          cursor_visible = false;
static cursor_type_t s_current_cursor_type = CURSOR_TYPE_POINTER;

/*
 * save_background:
 * Reads pixels underneath (cursor_x, cursor_y) into save_under array.
 * Reads from the off-screen back buffer if ready (guaranteed clean),
 * or directly from VRAM otherwise.
 */
static void save_background(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->virt_addr) return;

    uint32_t pitch_dwords = fb->pitch / 4;
    const uint32_t* src = gfx_is_ready() ? gfx_get_backbuffer() : (const uint32_t*)fb->virt_addr;
    if (!src) return;

    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = cursor_y + r;
        if (py < 0 || (uint32_t)py >= fb->height) {
            continue;
        }

        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = cursor_x + c;
            if (px < 0 || (uint32_t)px >= fb->width) {
                continue;
            }

            save_under[r][c] = src[(uint32_t)py * pitch_dwords + (uint32_t)px];
        }
    }
}

/*
 * restore_background:
 * Writes the saved pixels back to VRAM to erase the cursor.
 */
static void restore_background(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->virt_addr) return;

    uint32_t pitch_dwords = fb->pitch / 4;
    uint32_t* vram = (uint32_t*)fb->virt_addr;

    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = cursor_y + r;
        if (py < 0 || (uint32_t)py >= fb->height) {
            continue;
        }

        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = cursor_x + c;
            if (px < 0 || (uint32_t)px >= fb->width) {
                continue;
            }

            vram[(uint32_t)py * pitch_dwords + (uint32_t)px] = save_under[r][c];
        }
    }
}

/*
 * get_active_bitmap:
 * Selects the cursor matrix corresponding to the current cursor type.
 */
static const uint8_t (*get_active_bitmap(void))[CURSOR_WIDTH] {
    switch (s_current_cursor_type) {
        case CURSOR_TYPE_RESIZE_H:       return cursor_bitmap_resize_h;
        case CURSOR_TYPE_RESIZE_V:       return cursor_bitmap_resize_v;
        case CURSOR_TYPE_RESIZE_DIAG_BR: return cursor_bitmap_resize_diag_br;
        case CURSOR_TYPE_RESIZE_DIAG_BL: return cursor_bitmap_resize_diag_bl;
        case CURSOR_TYPE_POINTER:
        default:                         return cursor_bitmap;
    }
}

/*
 * draw_cursor:
 * Renders the 12x19 cursor sprite into VRAM, skipping transparent pixels.
 */
static void draw_cursor(void) {
    const fb_info_t* fb = fb_get_info();
    if (!fb || !fb->virt_addr) return;

    uint32_t pitch_dwords = fb->pitch / 4;
    uint32_t* vram = (uint32_t*)fb->virt_addr;
    const uint8_t (*bmp)[CURSOR_WIDTH] = get_active_bitmap();

    for (int r = 0; r < CURSOR_HEIGHT; r++) {
        int py = cursor_y + r;
        if (py < 0 || (uint32_t)py >= fb->height) {
            continue;
        }

        for (int c = 0; c < CURSOR_WIDTH; c++) {
            int px = cursor_x + c;
            if (px < 0 || (uint32_t)px >= fb->width) {
                continue;
            }

            uint8_t pixel_type = bmp[r][c];
            if (pixel_type == 1) {
                vram[(uint32_t)py * pitch_dwords + (uint32_t)px] = GFX_COLOR_BLACK;
            } else if (pixel_type == 2) {
                vram[(uint32_t)py * pitch_dwords + (uint32_t)px] = GFX_COLOR_WHITE;
            }
        }
    }
}

/*
 * cursor_init:
 * Initializes cursor position to screen center.
 */
void cursor_init(void) {
    const fb_info_t* fb = fb_get_info();
    if (fb && fb->width > 0 && fb->height > 0) {
        cursor_x = (int)fb->width / 2;
        cursor_y = (int)fb->height / 2;
    } else {
        cursor_x = 512;
        cursor_y = 384;
    }
    cursor_visible = false;
}

/*
 * cursor_show:
 * Saves background and draws cursor sprite to VRAM.
 */
void cursor_show(void) {
    if (cursor_visible) return;

    save_background();
    draw_cursor();
    cursor_visible = true;
}

/*
 * cursor_hide:
 * Restores background pixels to VRAM, hiding the cursor.
 */
void cursor_hide(void) {
    if (!cursor_visible) return;

    restore_background();
    cursor_visible = false;
}

/*
 * cursor_move:
 * Updates cursor position. Touches strictly the two 12x19 bounding boxes
 * (old area and new area), never triggering full-screen redraws.
 */
void cursor_move(int new_x, int new_y) {
    const fb_info_t* fb = fb_get_info();
    if (fb) {
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if ((uint32_t)new_x >= fb->width) new_x = (int)fb->width - 1;
        if ((uint32_t)new_y >= fb->height) new_y = (int)fb->height - 1;
    }

    if (new_x == cursor_x && new_y == cursor_y) {
        return;
    }

    if (!cursor_visible) {
        cursor_x = new_x;
        cursor_y = new_y;
        return;
    }

    /* 1. Restore old 12x19 background */
    restore_background();

    /* 2. Update coordinates */
    cursor_x = new_x;
    cursor_y = new_y;

    /* 3. Save new 12x19 background from back buffer */
    save_background();

    /* 4. Draw cursor sprite into new location */
    draw_cursor();
}

/*
 * cursor_is_visible:
 * Returns true if cursor is currently displayed.
 */
bool cursor_is_visible(void) {
    return cursor_visible;
}

/*
 * cursor_get_pos:
 * Returns current cursor coordinates.
 */
void cursor_get_pos(int* x, int* y) {
    if (x) *x = cursor_x;
    if (y) *y = cursor_y;
}

/*
 * cursor_overlaps_rect:
 * Checks if a dirty rectangle intersects the 12x19 cursor bounding box.
 */
bool cursor_overlaps_rect(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return false;

    return (x < cursor_x + CURSOR_WIDTH &&
            x + w > cursor_x &&
            y < cursor_y + CURSOR_HEIGHT &&
            y + h > cursor_y);
}

/*
 * cursor_set_type:
 * Switches the active cursor sprite shape (e.g. pointer vs resize bidirectional arrows).
 */
void cursor_set_type(cursor_type_t type) {
    if (type == s_current_cursor_type) return;

    if (cursor_visible) {
        restore_background();
    }
    s_current_cursor_type = type;
    if (cursor_visible) {
        save_background();
        draw_cursor();
    }
}

/*
 * cursor_get_type:
 * Returns the active cursor sprite shape type.
 */
cursor_type_t cursor_get_type(void) {
    return s_current_cursor_type;
}
