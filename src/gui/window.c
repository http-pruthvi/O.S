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
    win->bg_color = WIN_DEFAULT_CLIENT_BG;
    win->next = NULL;
    win->prev = NULL;
    win->first_widget = NULL;
    win->on_close = NULL;
    win->on_key = NULL;
    win->on_click = NULL;

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

    /* Title bar area (top 24px) */
    if (sy < win->y + WINDOW_TITLE_BAR_HEIGHT) {
        /* Check close button: positioned on right edge with 3px margin */
        int btn_x = win->x + win->width - WINDOW_CLOSE_BTN_SIZE - 4;
        int btn_y = win->y + (WINDOW_TITLE_BAR_HEIGHT - WINDOW_CLOSE_BTN_SIZE) / 2;

        if (sx >= btn_x && sx < btn_x + WINDOW_CLOSE_BTN_SIZE &&
            sy >= btn_y && sy < btn_y + WINDOW_CLOSE_BTN_SIZE) {
            return WINDOW_HIT_CLOSE_BUTTON;
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
 * Composites the window (frame, title bar, close button, client canvas)
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

    /* 1. Draw outer 2px decorative border */
    for (int r = win->y; r < win->y + win->height; r++) {
        if (r < clip_y || r >= clip_b) continue;

        bool is_tb_edge = (r < win->y + WINDOW_BORDER_WIDTH) ||
                          (r >= win->y + win->height - WINDOW_BORDER_WIDTH);

        for (int c = win->x; c < win->x + win->width; c++) {
            if (c < clip_x || c >= clip_r) continue;

            if (is_tb_edge || (c < win->x + WINDOW_BORDER_WIDTH) ||
                (c >= win->x + win->width - WINDOW_BORDER_WIDTH)) {
                screen_buffer[r * screen_pitch_dwords + c] = border_color;
            }
        }
    }

    /* 2. Draw Title Bar Background (inside 2px border) */
    int tb_start_y = win->y + WINDOW_BORDER_WIDTH;
    int tb_end_y   = win->y + WINDOW_TITLE_BAR_HEIGHT;
    int tb_start_x = win->x + WINDOW_BORDER_WIDTH;
    int tb_end_x   = win->x + win->width - WINDOW_BORDER_WIDTH;

    for (int r = tb_start_y; r < tb_end_y; r++) {
        if (r < clip_y || r >= clip_b) continue;
        uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
        for (int c = tb_start_x; c < tb_end_x; c++) {
            if (c < clip_x || c >= clip_r) continue;
            line[c] = title_bg;
        }
    }

    /* 3. Draw Title Text */
    int title_draw_x = win->x + WINDOW_BORDER_WIDTH + 6;
    int title_draw_y = win->y + 4;
    const char* t = win->title;
    while (*t && title_draw_x + FONT_GLYPH_WIDTH < tb_end_x - 26) {
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

    /* 4. Draw Close Button [X] */
    int btn_x = win->x + win->width - WINDOW_CLOSE_BTN_SIZE - 4;
    int btn_y = win->y + (WINDOW_TITLE_BAR_HEIGHT - WINDOW_CLOSE_BTN_SIZE) / 2;

    for (int r = btn_y; r < btn_y + WINDOW_CLOSE_BTN_SIZE; r++) {
        if (r < clip_y || r >= clip_b) continue;
        uint32_t* line = &screen_buffer[r * screen_pitch_dwords];
        for (int c = btn_x; c < btn_x + WINDOW_CLOSE_BTN_SIZE; c++) {
            if (c < clip_x || c >= clip_r) continue;

            if (r == btn_y || r == btn_y + WINDOW_CLOSE_BTN_SIZE - 1 ||
                c == btn_x || c == btn_x + WINDOW_CLOSE_BTN_SIZE - 1) {
                line[c] = WIN_CLOSE_BTN_BORDER;
            } else {
                line[c] = WIN_CLOSE_BTN_BG;
            }
        }
    }

    /* Draw 'X' glyph inside close button */
    int icon_x = btn_x + (WINDOW_CLOSE_BTN_SIZE - FONT_GLYPH_WIDTH) / 2;
    int icon_y = btn_y + (WINDOW_CLOSE_BTN_SIZE - FONT_GLYPH_HEIGHT) / 2;
    const uint8_t* x_glyph = font_get_glyph('x');
    for (uint32_t gr = 0; gr < FONT_GLYPH_HEIGHT; gr++) {
        int py = icon_y + (int)gr;
        if (py < clip_y || py >= clip_b) continue;

        uint8_t bits = x_glyph[gr];
        for (uint32_t gc = 0; gc < FONT_GLYPH_WIDTH; gc++) {
            int px = icon_x + (int)gc;
            if (px < clip_x || px >= clip_r) continue;

            if (bits & (0x80 >> gc)) {
                screen_buffer[py * screen_pitch_dwords + px] = WIN_CLOSE_BTN_TXT;
            }
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
