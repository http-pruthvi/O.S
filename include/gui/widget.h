#ifndef GUI_WIDGET_H
#define GUI_WIDGET_H

#include "gui/window.h"

/*
 * widget.h:
 * Reusable GUI Controls (Labels, Buttons, and Text Boxes).
 *
 * Why this is needed:
 * Windows require rich, interactive controls for user input and information
 * display. The widget framework provides reusable UI primitives that render
 * inside a window's client canvas and receive targeted mouse and keyboard events.
 */

#define WIDGET_TEXT_MAX 64

/* Widget Types */
typedef enum {
    WIDGET_TYPE_LABEL = 0,
    WIDGET_TYPE_BUTTON,
    WIDGET_TYPE_TEXTBOX
} widget_type_t;

/* Widget Structure */
typedef struct widget {
    uint32_t id;
    widget_type_t type;

    /* Geometry relative to window client area (0, 0 = top-left of client canvas) */
    int x;
    int y;
    int width;
    int height;

    /* Text label or input contents */
    char text[WIDGET_TEXT_MAX];

    /* Styling attributes */
    uint32_t fg_color;
    uint32_t bg_color;
    bool     transparent;
    bool     visible;
    bool     enabled;
    bool     focused;
    bool     pressed;

    /* Textbox caret position */
    size_t   cursor_pos;
    size_t   max_length;

    /* Callback functions */
    void (*on_click)(struct widget* w, void* user_data);
    void (*on_change)(struct widget* w, const char* text);
    void* user_data;

    /* Parent window */
    window_t* parent;

    /* Sibling widget linked list */
    struct widget* next;
} widget_t;

/*
 * Widget Factory Functions
 */

/* Creates a static text label widget */
widget_t* widget_create_label(int x, int y, int w, int h, const char* text, uint32_t fg_color);

/* Creates an interactive clickable push button widget */
widget_t* widget_create_button(int x, int y, int w, int h, const char* text,
                               void (*on_click)(widget_t*, void*), void* user_data);

/* Creates a single-line editable text input box widget */
widget_t* widget_create_textbox(int x, int y, int w, int h, const char* placeholder, size_t max_len);

/* Attaches a widget to a parent window */
void widget_add_to_window(window_t* win, widget_t* w);

/* Updates widget text and redraws into parent client canvas */
void widget_set_text(widget_t* w, const char* text);

/* Renders a single widget into its parent window's client canvas */
void widget_render(widget_t* w);

/* Renders all child widgets of a window */
void widget_render_all(window_t* win);

/* Dispatches client-relative mouse events to child widgets */
bool widget_dispatch_mouse(window_t* win, int rx, int ry, bool left_down, bool left_pressed, bool left_released);

/* Dispatches keyboard characters to the focused widget */
bool widget_dispatch_key(window_t* win, char c);

/* Destroys all widgets attached to a window */
void widget_destroy_all(window_t* win);

#endif /* GUI_WIDGET_H */
