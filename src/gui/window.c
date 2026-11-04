#include "gui/window.h"
#include "gui/theme.h"
#include "gfx/font.h"
#include "gfx/gfx.h"
#include "mm/heap.h"
#include "lib/string.h"

/* Color Palette for Window Chrome */
#define WIN_BORDER_ACTIVE       0x00456E9E
#define WIN_BORDER_INACTIVE     0x0032363E
#define WIN_TITLE_ACTIVE        0x001F487E
#define WIN_TITLE_INACTIVE      0x002B2E37
#define WIN_TITLE_TXT_ACTIVE    0x00FFFFFF
#define WIN_TITLE_TXT_INACTIVE  0x009BA3B0
#define WIN_CLOSE_BTN_BG        0x00C82828
#define WIN_CLOSE_BTN_BORDER    0x00E05050
#define WIN_CLOSE_BTN_TXT       0x00FFFFFF
#define WIN_DEFAULT_CLIENT_BG   0x00EAEAEA

static uint32_t next_window_id = 1;

/*
 * window_create:
 * Allocates a new window object with its own off-screen client canvas in RAM.
 */
window_t* window_create(int x, int y, int width, int height, const char* title, uint32_t flags) {
    if (width < 100) width = 100;
    if (height < 60) height = 60;

    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) {
        return NULL;
    }

    win->id = next_window_id++;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;

    /* Compute client area dimensions inside the decorative frame */
    win->client_x = x + WINDOW_BORDER_WIDTH;
    win->client_y = y + WINDOW_TITLE_BAR_HEIGHT;
    win->client_width = width - (2 * WINDOW_BORDER_WIDTH);
    win->client_height = height - WINDOW_TITLE_BAR_HEIGHT - WINDOW_BORDER_WIDTH;

    win->flags = flags | WINDOW_FLAG_VISIBLE;
    win->is_maximized = false;
    win->restore_x = x;
    win->restore_y = y;
    win->restore_width = width;
    win->restore_height = height;

    const theme_t* init_th = theme_get_current();
    win->bg_color = init_th ? init_th->bg_window_client : 0x001F1F1F;
    win->next = NULL;
    win->prev = NULL;
    win->first_widget = NULL;
    win->on_close = NULL;
    win->on_key = NULL;
    win->on_click = NULL;
    win->on_right_click = NULL;
    win->on_resize = NULL;

    /* Copy title safely */
    size_t i = 0;
    if (title) {
        while (title[i] && i < WINDOW_TITLE_MAX - 1) {
            win->title[i] = title[i];
            i++;
        }
    }
    win->title[i] = '\0';

    /* Allocate client surface pixel buffer */
    size_t client_pixels = (size_t)win->client_width * (size_t)win->client_height;
    win->client_buffer = (uint32_t*)kmalloc(client_pixels * sizeof(uint32_t));
    if (!win->client_buffer) {
        kfree(win);
        return NULL;
    }

    /* Initialize client canvas with default background color */
    window_clear_client(win, win->bg_color);

    return win;
}

/*
 * window_destroy:
 * Frees client surface memory, child widgets, and window container.
 */
void window_destroy(window_t* win) {
    if (!win) return;

    /* Free all attached child widgets */
    extern void widget_destroy_all(window_t* win);
    widget_destroy_all(win);

    if (win->client_buffer) {
        kfree(win->client_buffer);
        win->client_buffer = NULL;
    }

    kfree(win);
}

/*
 * window_clear_client:
 * Fills the window client buffer with a solid background color.
 */
void window_clear_client(window_t* win, uint32_t color) {
    if (!win || !win->client_buffer) return;

    size_t count = (size_t)win->client_width * (size_t)win->client_height;
    for (size_t i = 0; i < count; i++) {
        win->client_buffer[i] = color;
    }
}

/*
 * window_putpixel:
 * Draws a pixel into the client buffer with clipping.
 */
void window_putpixel(window_t* win, int cx, int cy, uint32_t color) {
    if (!win || !win->client_buffer) return;
    if (cx < 0 || cy < 0 || cx >= win->client_width || cy >= win->client_height) {
        return;
    }

    win->client_buffer[cy * win->client_width + cx] = color;
}

/*
 * window_fill_rect:
 * Fills a rectangle inside the window client canvas with boundary clipping.
 */
void window_fill_rect(window_t* win, int cx, int cy, int w, int h, uint32_t color) {
    if (!win || !win->client_buffer || w <= 0 || h <= 0) return;

    if (cx < 0) { w += cx; cx = 0; }
    if (cy < 0) { h += cy; cy = 0; }
    if (cx + w > win->client_width)  w = win->client_width - cx;
    if (cy + h > win->client_height) h = win->client_height - cy;

    if (w <= 0 || h <= 0) return;

    for (int r = cy; r < cy + h; r++) {
        uint32_t* line = &win->client_buffer[r * win->client_width + cx];
        for (int c = 0; c < w; c++) {
            line[c] = color;
        }
    }
}

/*
 * window_draw_rect:
 * Draws an un-filled outline rectangle in the client canvas.
 */
void window_draw_rect(window_t* win, int cx, int cy, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    window_fill_rect(win, cx, cy, w, 1, color);
    window_fill_rect(win, cx, cy + h - 1, w, 1, color);
    window_fill_rect(win, cx, cy, 1, h, color);
    window_fill_rect(win, cx + w - 1, cy, 1, h, color);
}

/*
 * window_draw_string:
 * Renders a string of text into the window client canvas using 8x16 font.
 */
void window_draw_string(window_t* win, int cx, int cy, const char* str,
                        uint32_t fg_color, uint32_t bg_color, bool transparent) {
    if (!win || !win->client_buffer || !str) return;

    int cur_cx = cx;
    while (*str) {
        char c = *str++;
        const uint8_t* glyph = font_get_glyph(c);

        for (uint32_t row = 0; row < FONT_GLYPH_HEIGHT; row++) {
            int py = cy + (int)row;
            if (py < 0 || py >= win->client_height) continue;

            uint8_t bits = glyph[row];
            for (uint32_t col = 0; col < FONT_GLYPH_WIDTH; col++) {
                int px = cur_cx + (int)col;
                if (px < 0 || px >= win->client_width) continue;

                if (bits & (0x80 >> col)) {
                    win->client_buffer[py * win->client_width + px] = fg_color;
                } else if (!transparent) {
                    win->client_buffer[py * win->client_width + px] = bg_color;
                }
            }
        }

        cur_cx += FONT_GLYPH_WIDTH;
    }
}

/*
 * window_hit_test:
 * Checks which region of a window contains the screen coordinate (sx, sy).
 */
window_hit_t window_hit_test(window_t* win, int sx, int sy) {
    if (!win || !(win->flags & WINDOW_FLAG_VISIBLE)) {
        return WINDOW_HIT_NONE;
    }

    /* Out of window bounds */
    if (sx < win->x || sx >= win->x + win->width ||
        sy < win->y || sy >= win->y + win->height) {
        return WINDOW_HIT_NONE;
    }

    /* Resize border hit testing (active only for resizable, non-maximized windows) */
    if ((win->flags & WINDOW_FLAG_RESIZABLE) && !win->is_maximized) {
        /* Bottom-Right corner (10px corner hit box) */
        if (sx >= win->x + win->width - 10 && sy >= win->y + win->height - 10) {
            return WINDOW_HIT_RESIZE_BOTTOM_RIGHT;
        }
        /* Bottom-Left corner (10px corner hit box) */
        if (sx < win->x + 10 && sy >= win->y + win->height - 10) {
            return WINDOW_HIT_RESIZE_BOTTOM_LEFT;
        }
        /* Bottom edge (5px border hit box) */
        if (sy >= win->y + win->height - 5) {
            return WINDOW_HIT_RESIZE_BOTTOM;
        }
        /* Left edge (5px border hit box) */
        if (sx < win->x + 5) {
            return WINDOW_HIT_RESIZE_LEFT;
        }
        /* Right edge (5px border hit box; only below title bar to avoid masking close button) */
        if (sy >= win->y + WINDOW_TITLE_BAR_HEIGHT && sx >= win->x + win->width - 5) {
            return WINDOW_HIT_RESIZE_RIGHT;
        }
    }

    /* Title bar area (top 28px) */
    if (sy < win->y + WINDOW_TITLE_BAR_HEIGHT) {
        /* Windows 11 Caption Controls:
         * Close button: rightmost 38px
         * Maximize button: 34px
         * Minimize button: 34px
         */
        if (sx >= win->x + win->width - 38 && sx < win->x + win->width) {
            return WINDOW_HIT_CLOSE_BUTTON;
        }
        if (sx >= win->x + win->width - 72 && sx < win->x + win->width - 38) {
            return WINDOW_HIT_MAX_BUTTON;
        }
        if (sx >= win->x + win->width - 106 && sx < win->x + win->width - 72) {
            return WINDOW_HIT_MIN_BUTTON;
        }

        return WINDOW_HIT_TITLE_BAR;
    }

    /* Inside client rect */
    if (sx >= win->client_x && sx < win->client_x + win->client_width &&
        sy >= win->client_y && sy < win->client_y + win->client_height) {
        return WINDOW_HIT_CLIENT;
    }

    return WINDOW_HIT_BORDER;
}

/*
 * window_render:
 * Composites the window (frame, title bar, caption controls, client canvas)
 * into the destination screen back buffer with clipping.
 */
void window_render(window_t* win, uint32_t* screen_buffer, uint32_t screen_pitch_dwords,
                   int clip_x, int clip_y, int clip_w, int clip_h) {
    if (!win || !screen_buffer || !(win->flags & WINDOW_FLAG_VISIBLE)) {
        return;
    }

    /* Check bounding box intersection with clipping region */
    int win_r = win->x + win->width;
    int win_b = win->y + win->height;
    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    if (win->x >= clip_r || win_r <= clip_x || win->y >= clip_b || win_b <= clip_y) {
        return; /* No overlap */
    }

    bool focused = (win->flags & WINDOW_FLAG_FOCUSED) != 0;
    const theme_t* th = theme_get_current();
    uint32_t border_color = focused ? (th ? th->border_active : WIN_BORDER_ACTIVE)
                                    : (th ? th->border_inactive : WIN_BORDER_INACTIVE);
    uint32_t title_bg     = focused ? (th ? th->title_active_bg : WIN_TITLE_ACTIVE)
                                    : (th ? th->title_inactive_bg : WIN_TITLE_INACTIVE);
    uint32_t title_fg     = focused ? (th ? th->title_active_fg : WIN_TITLE_TXT_ACTIVE)
                                    : (th ? th->title_inactive_fg : WIN_TITLE_TXT_INACTIVE);

    /* 1. Draw outer 1px clean border */
    for (int r = win->y; r < win->y + win->height; r++) {
        if (r < clip_y || r >= clip_b) continue;

        bool is_tb_edge = (r == win->y) || (r == win->y + win->height - 1);

        for (int c = win->x; c < win->x + win->width; c++) {
            if (c < clip_x || c >= clip_r) continue;

            if (is_tb_edge || (c == win->x) || (c == win->x + win->width - 1)) {
                screen_buffer[r * screen_pitch_dwords + c] = border_color;
            }
        }
    }

    /* 2. Draw Title Bar Background (inside 1px border) */
    int tb_start_y = win->y + 1;
    int tb_end_y   = win->y + WINDOW_TITLE_BAR_HEIGHT;
    int tb_start_x = win->x + 1;
    int tb_end_x   = win->x + win->width - 1;

    for (int r = tb_start_y; r < tb_end_y; r++) {
        if (r < clip_y || r >= clip_b) continue;
        uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
        for (int c = tb_start_x; c < tb_end_x; c++) {
            if (c < clip_x || c >= clip_r) continue;
            line[c] = title_bg;
        }
    }

    /* Subtle separator line under title bar */
    int sep_y = win->y + WINDOW_TITLE_BAR_HEIGHT - 1;
    if (sep_y >= clip_y && sep_y < clip_b) {
        uint32_t* line = &screen_buffer[sep_y * screen_pitch_dwords];
        uint32_t sep_col = focused ? (th ? th->border_active : 0x000078D4) : 0x002B2B2B;
        for (int c = tb_start_x; c < tb_end_x; c++) {
            if (c >= clip_x && c < clip_r) {
                line[c] = sep_col;
            }
        }
    }

    /* 3. Draw Title Text */
    int title_draw_x = win->x + 10;
    int title_draw_y = win->y + (WINDOW_TITLE_BAR_HEIGHT - FONT_GLYPH_HEIGHT) / 2;
    const char* t = win->title;
    while (*t && title_draw_x + FONT_GLYPH_WIDTH < win->x + win->width - 115) {
        const uint8_t* glyph = font_get_glyph(*t++);
        for (uint32_t gr = 0; gr < FONT_GLYPH_HEIGHT; gr++) {
            int py = title_draw_y + (int)gr;
            if (py < clip_y || py >= clip_b) continue;

            uint8_t bits = glyph[gr];
            for (uint32_t gc = 0; gc < FONT_GLYPH_WIDTH; gc++) {
                int px = title_draw_x + (int)gc;
                if (px < clip_x || px >= clip_r) continue;

                if (bits & (0x80 >> gc)) {
                    screen_buffer[py * screen_pitch_dwords + px] = title_fg;
                }
            }
        }
        title_draw_x += FONT_GLYPH_WIDTH;
    }

    /* 4. Draw Windows 11 Caption Buttons */
    int btn_cy = win->y + WINDOW_TITLE_BAR_HEIGHT / 2;

    /* Minimize [ — ] button */
    int min_btn_x = win->x + win->width - 106;
    int min_btn_w = 34;
    int min_line_y = btn_cy + 3;
    if (min_line_y >= clip_y && min_line_y < clip_b) {
        uint32_t* line = &screen_buffer[min_line_y * screen_pitch_dwords];
        int min_line_x = min_btn_x + (min_btn_w - 10) / 2;
        for (int c = min_line_x; c < min_line_x + 10; c++) {
            if (c >= clip_x && c < clip_r) {
                line[c] = title_fg;
            }
        }
    }

    /* Maximize [ □ ] or Restore [ ❐ ] button */
    int max_btn_x = win->x + win->width - 72;
    int max_btn_w = 34;
    int max_box_x = max_btn_x + (max_btn_w - 10) / 2;
    int max_box_y = btn_cy - 5;

    if (!win->is_maximized) {
        /* Single square [ □ ] (10x10 outline) */
        for (int r = max_box_y; r < max_box_y + 10; r++) {
            if (r < clip_y || r >= clip_b) continue;
            uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
            for (int c = max_box_x; c < max_box_x + 10; c++) {
                if (c < clip_x || c >= clip_r) continue;
                if (r == max_box_y || r == max_box_y + 9 || c == max_box_x || c == max_box_x + 9) {
                    line[c] = title_fg;
                }
            }
        }
    } else {
        /* Overlapping restore icon [ ❐ ]: Back offset square + Front square */
        /* Back square: top-right (8x8) at (max_box_x + 2, max_box_y - 1) */
        for (int r = max_box_y - 1; r < max_box_y + 7; r++) {
            if (r < clip_y || r >= clip_b) continue;
            uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
            for (int c = max_box_x + 2; c < max_box_x + 10; c++) {
                if (c < clip_x || c >= clip_r) continue;
                if (r == max_box_y - 1 || c == max_box_x + 9 || (r == max_box_y + 6 && c >= max_box_x + 7)) {
                    line[c] = title_fg;
                }
            }
        }
        /* Front square: bottom-left (8x8) at (max_box_x - 1, max_box_y + 2) */
        for (int r = max_box_y + 2; r < max_box_y + 10; r++) {
            if (r < clip_y || r >= clip_b) continue;
            uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
            for (int c = max_box_x - 1; c < max_box_x + 7; c++) {
                if (c < clip_x || c >= clip_r) continue;
                if (r == max_box_y + 2 || r == max_box_y + 9 || c == max_box_x - 1 || c == max_box_x + 6) {
                    line[c] = title_fg;
                }
            }
        }
    }

    /* Close [ ✕ ] button */
    int close_btn_x = win->x + win->width - 38;
    int close_btn_w = 37;
    int close_btn_y = win->y + 1;
    int close_btn_h = WINDOW_TITLE_BAR_HEIGHT - 2;

    /* Close button hover/active background (Windows 11 red when focused) */
    uint32_t close_bg = focused ? (th ? th->btn_close_bg : 0x00C42B1C) : title_bg;
    for (int r = close_btn_y; r < close_btn_y + close_btn_h; r++) {
        if (r < clip_y || r >= clip_b) continue;
        uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
        for (int c = close_btn_x; c < close_btn_x + close_btn_w; c++) {
            if (c < clip_x || c >= clip_r) continue;
            line[c] = close_bg;
        }
    }

    /* Crisp Vector Diagonal X */
    int close_cx = close_btn_x + close_btn_w / 2;
    int close_cy = close_btn_y + close_btn_h / 2;
    for (int d = -4; d <= 4; d++) {
        int py = close_cy + d;
        if (py < clip_y || py >= clip_b) continue;
        int px1 = close_cx + d;
        int px2 = close_cx - d;
        if (px1 >= clip_x && px1 < clip_r) {
            screen_buffer[py * screen_pitch_dwords + px1] = 0x00FFFFFF;
        }
        if (px2 >= clip_x && px2 < clip_r) {
            screen_buffer[py * screen_pitch_dwords + px2] = 0x00FFFFFF;
        }
    }

    /* 5. Blit Client Surface into Destination Buffer */
    if (win->client_buffer) {
        int src_y_start = 0;
        int dst_y_start = win->client_y;
        int render_h   = win->client_height;

        /* Clip vertically */
        if (dst_y_start < clip_y) {
            int diff = clip_y - dst_y_start;
            src_y_start += diff;
            dst_y_start = clip_y;
            render_h -= diff;
        }
        if (dst_y_start + render_h > clip_b) {
            render_h = clip_b - dst_y_start;
        }

        int src_x_start = 0;
        int dst_x_start = win->client_x;
        int render_w   = win->client_width;

        /* Clip horizontally */
        if (dst_x_start < clip_x) {
            int diff = clip_x - dst_x_start;
            src_x_start += diff;
            dst_x_start = clip_x;
            render_w -= diff;
        }
        if (dst_x_start + render_w > clip_r) {
            render_w = clip_r - dst_x_start;
        }

        if (render_w > 0 && render_h > 0) {
            for (int r = 0; r < render_h; r++) {
                uint32_t* src_line = &win->client_buffer[(src_y_start + r) * win->client_width + src_x_start];
                uint32_t* dst_line = &screen_buffer[(dst_y_start + r) * screen_pitch_dwords + dst_x_start];
                memcpy(dst_line, src_line, (size_t)render_w * sizeof(uint32_t));
            }
        }
    }
}

/*
 * window_resize:
 * Updates the window outer and client dimensions, frees and reallocates the
 * off-screen client buffer, and triggers the on_resize callback.
 */
void window_resize(window_t* win, int new_w, int new_h) {
    if (!win) return;
    if (new_w < 100) new_w = 100;
    if (new_h < 60) new_h = 60;

    /* Calculate new client dimensions */
    int new_cw = new_w - (2 * WINDOW_BORDER_WIDTH);
    int new_ch = new_h - WINDOW_TITLE_BAR_HEIGHT - WINDOW_BORDER_WIDTH;
    if (new_cw < 10) new_cw = 10;
    if (new_ch < 10) new_ch = 10;

    /* Reallocate client surface pixel buffer */
    size_t new_pixels = (size_t)new_cw * (size_t)new_ch;
    uint32_t* new_buf = (uint32_t*)kmalloc(new_pixels * sizeof(uint32_t));
    if (new_buf) {
        if (win->client_buffer) {
            kfree(win->client_buffer);
        }
        win->client_buffer = new_buf;
        win->width = new_w;
        win->height = new_h;
        win->client_width = new_cw;
        win->client_height = new_ch;
        win->client_x = win->x + WINDOW_BORDER_WIDTH;
        win->client_y = win->y + WINDOW_TITLE_BAR_HEIGHT;

        window_clear_client(win, win->bg_color);
    } else {
        return;
    }

    /* Trigger window-specific relayout / redraw if window has on_resize callback */
    if (win->on_resize) {
        win->on_resize(win, new_w, new_h);
    }
}

/*
 * window_set_title:
 * Updates the title string of a window.
 */
void window_set_title(window_t* win, const char* title) {
    if (!win) return;
    if (title) {
        strncpy(win->title, title, WINDOW_TITLE_MAX - 1);
        win->title[WINDOW_TITLE_MAX - 1] = '\0';
    } else {
        win->title[0] = '\0';
    }
}
