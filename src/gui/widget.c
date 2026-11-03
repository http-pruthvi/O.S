#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "mm/heap.h"
#include "lib/string.h"

static uint32_t next_widget_id = 1;

/*
 * widget_create_label:
 * Creates a static text label widget.
 */
widget_t* widget_create_label(int x, int y, int w, int h, const char* text, uint32_t fg_color) {
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;

    widget->id = next_widget_id++;
    widget->type = WIDGET_TYPE_LABEL;
    widget->x = x;
    widget->y = y;
    widget->width = w;
    widget->height = h;

    widget->fg_color = fg_color;
    widget->bg_color = 0x00EAEAEA;
    widget->transparent = true;
    widget->visible = true;
    widget->enabled = true;
    widget->focused = false;
    widget->pressed = false;
    widget->cursor_pos = 0;
    widget->max_length = WIDGET_TEXT_MAX;

    widget->on_click = NULL;
    widget->on_change = NULL;
    widget->user_data = NULL;
    widget->parent = NULL;
    widget->next = NULL;

    size_t i = 0;
    if (text) {
        while (text[i] && i < WIDGET_TEXT_MAX - 1) {
            widget->text[i] = text[i];
            i++;
        }
    }
    widget->text[i] = '\0';

    return widget;
}

/*
 * widget_create_button:
 * Creates a clickable push button widget with 3D beveled relief.
 */
widget_t* widget_create_button(int x, int y, int w, int h, const char* text,
                               void (*on_click)(widget_t*, void*), void* user_data) {
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;

    widget->id = next_widget_id++;
    widget->type = WIDGET_TYPE_BUTTON;
    widget->x = x;
    widget->y = y;
    widget->width = w;
    widget->height = h;

    widget->fg_color = 0x00FFFFFF; /* Crisp white text */
    widget->bg_color = 0x002D2D2D; /* Modern dark flat button surface */
    widget->transparent = false;
    widget->visible = true;
    widget->enabled = true;
    widget->focused = false;
    widget->pressed = false;
    widget->cursor_pos = 0;
    widget->max_length = WIDGET_TEXT_MAX;

    widget->on_click = on_click;
    widget->on_change = NULL;
    widget->user_data = user_data;
    widget->parent = NULL;
    widget->next = NULL;

    size_t i = 0;
    if (text) {
        while (text[i] && i < WIDGET_TEXT_MAX - 1) {
            widget->text[i] = text[i];
            i++;
        }
    }
    widget->text[i] = '\0';

    return widget;
}

/*
 * widget_create_textbox:
 * Creates an interactive text input box widget.
 */
widget_t* widget_create_textbox(int x, int y, int w, int h, const char* placeholder, size_t max_len) {
    widget_t* widget = (widget_t*)kmalloc(sizeof(widget_t));
    if (!widget) return NULL;

    widget->id = next_widget_id++;
    widget->type = WIDGET_TYPE_TEXTBOX;
    widget->x = x;
    widget->y = y;
    widget->width = w;
    widget->height = h;

    widget->fg_color = 0x00F0F0F0; /* Crisp white text */
    widget->bg_color = 0x00181818; /* Modern dark background */
    widget->transparent = false;
    widget->visible = true;
    widget->enabled = true;
    widget->focused = false;
    widget->pressed = false;
    widget->cursor_pos = 0;
    widget->max_length = (max_len < WIDGET_TEXT_MAX) ? max_len : WIDGET_TEXT_MAX - 1;

    widget->on_click = NULL;
    widget->on_change = NULL;
    widget->user_data = NULL;
    widget->parent = NULL;
    widget->next = NULL;

    size_t i = 0;
    if (placeholder) {
        while (placeholder[i] && i < widget->max_length) {
            widget->text[i] = placeholder[i];
            i++;
        }
    }
    widget->text[i] = '\0';
    widget->cursor_pos = i;

    return widget;
}

/*
 * widget_add_to_window:
 * Appends a widget to a window's widget list and triggers its initial rendering.
 */
void widget_add_to_window(window_t* win, widget_t* w) {
    if (!win || !w) return;

    w->parent = win;
    w->next = NULL;

    if (!win->first_widget) {
        win->first_widget = w;
    } else {
        widget_t* curr = win->first_widget;
        while (curr->next) {
            curr = curr->next;
        }
        curr->next = w;
    }

    widget_render(w);
}

/*
 * widget_set_text:
 * Modifies widget text and refreshes its rendering inside parent canvas.
 */
void widget_set_text(widget_t* w, const char* text) {
    if (!w) return;

    size_t i = 0;
    if (text) {
        while (text[i] && i < w->max_length - 1) {
            w->text[i] = text[i];
            i++;
        }
    }
    w->text[i] = '\0';
    w->cursor_pos = i;

    widget_render(w);
    if (w->parent) {
        wm_invalidate_rect(w->parent->client_x + w->x,
                           w->parent->client_y + w->y,
                           w->width, w->height);
    }
}

/*
 * widget_render:
 * Renders the widget directly into its parent window's client canvas.
 */
void widget_render(widget_t* w) {
    if (!w || !w->visible || !w->parent) return;

    window_t* win = w->parent;

    if (w->type == WIDGET_TYPE_LABEL) {
        if (!w->transparent) {
            window_fill_rect(win, w->x, w->y, w->width, w->height, w->bg_color);
        }
        window_draw_string(win, w->x, w->y, w->text, w->fg_color, w->bg_color, w->transparent);
    } else if (w->type == WIDGET_TYPE_BUTTON) {
        /* Modern Flat Windows Button Background */
        uint32_t btn_bg = w->pressed ? 0x001F1F1F : w->bg_color;
        window_fill_rect(win, w->x, w->y, w->width, w->height, btn_bg);

        /* Crisp 1px flat border (accent blue if focused, subtle dark grey otherwise) */
        uint32_t btn_border = w->focused ? 0x000078D4 : (w->pressed ? 0x00555555 : 0x003D3D3D);
        window_draw_rect(win, w->x, w->y, w->width, w->height, btn_border);

        /* Center text inside button (shifted by 1px if pressed) */
        size_t text_len = strlen(w->text);
        int text_x = w->x + (w->width - (int)text_len * FONT_GLYPH_WIDTH) / 2;
        int text_y = w->y + (w->height - FONT_GLYPH_HEIGHT) / 2;
        if (w->pressed) {
            text_x += 1;
            text_y += 1;
        }

        window_draw_string(win, text_x, text_y, w->text, w->fg_color, btn_bg, true);
    } else if (w->type == WIDGET_TYPE_TEXTBOX) {
        /* Modern Flat Windows TextBox Background */
        window_fill_rect(win, w->x, w->y, w->width, w->height, w->bg_color);

        /* Border: Accent blue highlight if focused, subtle dark grey otherwise */
        uint32_t border_color = w->focused ? 0x000078D4 : 0x00333333;
        window_draw_rect(win, w->x, w->y, w->width, w->height, border_color);

        /* Fluent accent underline if focused */
        if (w->focused && w->height > 4) {
            window_fill_rect(win, w->x, w->y + w->height - 2, w->width, 2, 0x000078D4);
        }

        /* Draw Text */
        int text_x = w->x + 6;
        int text_y = w->y + (w->height - FONT_GLYPH_HEIGHT) / 2;
        window_draw_string(win, text_x, text_y, w->text, w->fg_color, w->bg_color, true);

        /* Draw White Caret if focused */
        if (w->focused) {
            int caret_x = text_x + (int)strlen(w->text) * FONT_GLYPH_WIDTH;
            if (caret_x < w->x + w->width - 6) {
                window_fill_rect(win, caret_x, text_y, 2, FONT_GLYPH_HEIGHT, 0x00FFFFFF);
            }
        }
    }
}

/*
 * widget_render_all:
 * Renders all child widgets of a window in order.
 */
void widget_render_all(window_t* win) {
    if (!win) return;

    widget_t* curr = win->first_widget;
    while (curr) {
        widget_render(curr);
        curr = curr->next;
    }
}

/*
 * widget_dispatch_mouse:
 * Delivers mouse interactions to the appropriate child widget based on hit testing.
 */
bool widget_dispatch_mouse(window_t* win, int rx, int ry, bool left_down, bool left_pressed, bool left_released) {
    if (!win) return false;

    bool handled = false;
    widget_t* curr = win->first_widget;

    while (curr) {
        if (!curr->visible || !curr->enabled) {
            curr = curr->next;
            continue;
        }

        bool inside = (rx >= curr->x && rx < curr->x + curr->width &&
                       ry >= curr->y && ry < curr->y + curr->height);

        if (curr->type == WIDGET_TYPE_BUTTON) {
            if (inside && left_pressed) {
                curr->pressed = true;
                widget_render(curr);
                wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                   curr->width, curr->height);
                handled = true;
            } else if (curr->pressed && left_released) {
                curr->pressed = false;
                widget_render(curr);
                wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                   curr->width, curr->height);

                if (inside && curr->on_click) {
                    curr->on_click(curr, curr->user_data);
                }
                handled = true;
            } else if (curr->pressed && !inside && left_down) {
                curr->pressed = false;
                widget_render(curr);
                wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                   curr->width, curr->height);
            }
        } else if (curr->type == WIDGET_TYPE_TEXTBOX) {
            if (left_pressed) {
                bool old_focus = curr->focused;
                curr->focused = inside;
                if (old_focus != curr->focused) {
                    widget_render(curr);
                    wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                       curr->width, curr->height);
                }
                if (inside) handled = true;
            }
        }

        curr = curr->next;
    }

    return handled;
}

/*
 * widget_dispatch_key:
 * Routes a typed character to the currently focused text input box.
 */
bool widget_dispatch_key(window_t* win, char c) {
    if (!win) return false;

    widget_t* curr = win->first_widget;
    while (curr) {
        if (curr->visible && curr->enabled && curr->focused && curr->type == WIDGET_TYPE_TEXTBOX) {
            size_t len = strlen(curr->text);

            if (c == '\b') {
                /* Backspace */
                if (len > 0) {
                    curr->text[len - 1] = '\0';
                    curr->cursor_pos = len - 1;
                    widget_render(curr);
                    wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                       curr->width, curr->height);
                    if (curr->on_change) {
                        curr->on_change(curr, curr->text);
                    }
                    return true;
                }
            } else if (c >= 32 && c <= 126) {
                /* Printable ASCII Character */
                if (len < curr->max_length) {
                    curr->text[len] = c;
                    curr->text[len + 1] = '\0';
                    curr->cursor_pos = len + 1;
                    widget_render(curr);
                    wm_invalidate_rect(win->client_x + curr->x, win->client_y + curr->y,
                                       curr->width, curr->height);
                    if (curr->on_change) {
                        curr->on_change(curr, curr->text);
                    }
                    return true;
                }
            }
        }
        curr = curr->next;
    }

    return false;
}

/*
 * widget_destroy_all:
 * Deallocates all widgets attached to a window.
 */
void widget_destroy_all(window_t* win) {
    if (!win) return;

    widget_t* curr = win->first_widget;
    while (curr) {
        widget_t* next = curr->next;
        kfree(curr);
        curr = next;
    }
    win->first_widget = NULL;
}
