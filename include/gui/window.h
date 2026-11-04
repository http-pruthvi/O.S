#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include "lib/types.h"

/*
 * window.h:
 * Desktop Window Representation and Client Surface.
 *
 * Why this is needed:
 * In a graphical windowing system, a window is the primary container for
 * applications and user interaction. Each window manages its position, dimensions,
 * visual state (focused, minimized), title bar decorations (drag handle, close button),
 * and an isolated off-screen client canvas for tear-free application rendering.
 */

#define WINDOW_TITLE_MAX          32
#define WINDOW_TITLE_BAR_HEIGHT   28
#define WINDOW_BORDER_WIDTH       1
#define WINDOW_CLOSE_BTN_SIZE     18

/* Window State Flags */
#define WINDOW_FLAG_VISIBLE       (1 << 0)
#define WINDOW_FLAG_FOCUSED       (1 << 1)
#define WINDOW_FLAG_MOVABLE       (1 << 2)
#define WINDOW_FLAG_RESIZABLE     (1 << 3)
#define WINDOW_FLAG_MINIMIZED     (1 << 4)

/* Hit Test Region Codes */
typedef enum {
    WINDOW_HIT_NONE = 0,
    WINDOW_HIT_TITLE_BAR,
    WINDOW_HIT_CLOSE_BUTTON,
    WINDOW_HIT_MAX_BUTTON,
    WINDOW_HIT_MIN_BUTTON,
    WINDOW_HIT_CLIENT,
    WINDOW_HIT_BORDER,
    WINDOW_HIT_RESIZE_LEFT,
    WINDOW_HIT_RESIZE_RIGHT,
    WINDOW_HIT_RESIZE_BOTTOM,
    WINDOW_HIT_RESIZE_BOTTOM_RIGHT,
    WINDOW_HIT_RESIZE_BOTTOM_LEFT
} window_hit_t;

/* Forward declaration */
struct widget;

/* Window Object Structure */
typedef struct window {
    uint32_t id;
    char     title[WINDOW_TITLE_MAX];

    /* Window outer geometry (including frame and title bar) */
    int x;
    int y;
    int width;
    int height;

    /* Window client area geometry (inside borders and title bar) */
    int client_x;
    int client_y;
    int client_width;
    int client_height;

    /* Maximize & Restore Geometry */
    bool is_maximized;
    int  restore_x;
    int  restore_y;
    int  restore_width;
    int  restore_height;

    /* State flags (WINDOW_FLAG_*) */
    uint32_t flags;

    /* Client surface pixel buffer in RAM (client_width * client_height dwords) */
    uint32_t* client_buffer;
    uint32_t  bg_color;

    /* Linked list pointers for Z-order stacking (top-to-bottom) */
    struct window* next; /* Window underneath this window */
    struct window* prev; /* Window above this window */

    /* Child widget list linked head */
    struct widget* first_widget;

    /* User callback invoked when window is requested to close */
    void (*on_close)(struct window* win);

    /* Optional user callback invoked when a key is pressed while window has focus */
    void (*on_key)(struct window* win, char c);

    /* Optional user callback invoked when window client area is clicked */
    void (*on_click)(struct window* win, int rx, int ry);

    /* Optional user callback invoked when window client area is right-clicked */
    void (*on_right_click)(struct window* win, int rx, int ry);

    /* Optional user callback invoked when window is resized */
    void (*on_resize)(struct window* win, int new_w, int new_h);
} window_t;

/*
 * Window Management API
 */

/* Creates a new window with allocated client canvas */
window_t* window_create(int x, int y, int width, int height, const char* title, uint32_t flags);

/* Resizes window outer geometry, reallocates client buffer, and notifies on_resize */
void window_resize(window_t* win, int new_w, int new_h);

/* Updates the window title and invalidates the title bar */
void window_set_title(window_t* win, const char* title);

/* Destroys a window, frees its client buffer and associated resources */
void window_destroy(window_t* win);

/* Clears the window client area with its default or specified background color */
void window_clear_client(window_t* win, uint32_t color);

/* Draws a pixel directly into the window client canvas at relative coordinates (cx, cy) */
void window_putpixel(window_t* win, int cx, int cy, uint32_t color);

/* Fills a rectangle inside the window client canvas */
void window_fill_rect(window_t* win, int cx, int cy, int w, int h, uint32_t color);

/* Draws an outline rectangle inside the window client canvas */
void window_draw_rect(window_t* win, int cx, int cy, int w, int h, uint32_t color);

/* Draws a text string inside the window client canvas */
void window_draw_string(window_t* win, int cx, int cy, const char* str, uint32_t fg_color, uint32_t bg_color, bool transparent);

/* Performs hit testing for a screen coordinate (sx, sy) against window boundaries */
window_hit_t window_hit_test(window_t* win, int sx, int sy);

/* Renders window frame, title bar, close button, and client area into the target screen buffer */
void window_render(window_t* win, uint32_t* screen_buffer, uint32_t screen_pitch_dwords,
                   int clip_x, int clip_y, int clip_w, int clip_h);

#endif /* GUI_WINDOW_H */

