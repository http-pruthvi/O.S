#include "apps/editor.h"
#include "fs/vfs.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/serial.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define EDITOR_MAX_LINES      64
#define EDITOR_MAX_COLS       60
#define EDITOR_LINE_HEIGHT    18
#define EDITOR_VISIBLE_LINES  15

#define EDITOR_CANVAS_X       10
#define EDITOR_CANVAS_Y       38
#define EDITOR_CANVAS_W       496
#define EDITOR_CANVAS_H       274

/* Text Editor State */
static window_t* s_editor_win = NULL;
static widget_t* s_lbl_status = NULL;
static char      s_current_filepath[64] = "/untitled.txt";

static char s_lines[EDITOR_MAX_LINES][EDITOR_MAX_COLS];
static int  s_line_lens[EDITOR_MAX_LINES];
static int  s_num_lines   = 1;
static int  s_cursor_row  = 0;
static int  s_cursor_col  = 0;
static int  s_scroll_top  = 0;

/* Helper: converts integer to string */
static void int_to_str(int val, char* buf) {
    int idx = 0;
    if (val < 0) {
        buf[idx++] = '-';
        val = -val;
    }
    if (val == 0) {
        buf[idx++] = '0';
        buf[idx] = '\0';
        return;
    }
    char temp[16];
    int t_idx = 0;
    while (val > 0) {
        temp[t_idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = t_idx - 1; i >= 0; i--) {
        buf[idx++] = temp[i];
    }
    buf[idx] = '\0';
}

/* Counts total characters across all lines */
static int get_total_chars(void) {
    int count = 0;
    for (int i = 0; i < s_num_lines; i++) {
        count += s_line_lens[i];
    }
    return count;
}

/* Updates status bar label with row, column, and character metrics */
static void update_status_bar(void) {
    if (!s_lbl_status) return;

    char status[80] = "Ln ";
    int idx = 3;
    int_to_str(s_cursor_row + 1, status + idx);
    while (status[idx]) idx++;

    status[idx++] = ','; status[idx++] = ' ';
    status[idx++] = 'C'; status[idx++] = 'o'; status[idx++] = 'l'; status[idx++] = ' ';
    int_to_str(s_cursor_col + 1, status + idx);
    while (status[idx]) idx++;

    status[idx++] = ' '; status[idx++] = '|'; status[idx++] = ' ';
    status[idx++] = 'L'; status[idx++] = 'i'; status[idx++] = 'n'; status[idx++] = 'e'; status[idx++] = 's';
    status[idx++] = ':'; status[idx++] = ' ';
    int_to_str(s_num_lines, status + idx);
    while (status[idx]) idx++;

    status[idx++] = ' '; status[idx++] = '|'; status[idx++] = ' ';
    status[idx++] = 'C'; status[idx++] = 'h'; status[idx++] = 'a'; status[idx++] = 'r'; status[idx++] = 's';
    status[idx++] = ':'; status[idx++] = ' ';
    int_to_str(get_total_chars(), status + idx);
    while (status[idx]) idx++;
    status[idx] = '\0';

    widget_set_text(s_lbl_status, status);
}

/*
 * redraw_editor_canvas:
 * Redraws text document lines and caret cursor in the editor client canvas.
 */
static void redraw_editor_canvas(void) {
    if (!s_editor_win) return;

    int canvas_w = s_editor_win->client_width - 20;
    int canvas_h = s_editor_win->client_height - 60;
    if (canvas_w < 60) canvas_w = 60;
    if (canvas_h < 40) canvas_h = 40;
    int visible_lines = (canvas_h - 12) / EDITOR_LINE_HEIGHT;
    if (visible_lines < 1) visible_lines = 1;

    /* 1. Clear text background to clean white */
    window_fill_rect(s_editor_win, EDITOR_CANVAS_X, EDITOR_CANVAS_Y,
                     canvas_w, canvas_h, 0x00FFFFFF);

    /* 2. Sunken frame border */
    window_draw_rect(s_editor_win, EDITOR_CANVAS_X, EDITOR_CANVAS_Y,
                     canvas_w, canvas_h, 0x00808080);

    /* 3. Adjust scroll offset if cursor has moved outside visible window */
    if (s_cursor_row < s_scroll_top) {
        s_scroll_top = s_cursor_row;
    } else if (s_cursor_row >= s_scroll_top + visible_lines) {
        s_scroll_top = s_cursor_row - visible_lines + 1;
    }

    /* 4. Draw visible text lines */
    int text_start_x = EDITOR_CANVAS_X + 6;
    int text_start_y = EDITOR_CANVAS_Y + 6;

    for (int i = 0; i < visible_lines; i++) {
        int line_idx = s_scroll_top + i;
        if (line_idx >= s_num_lines) break;

        int row_y = text_start_y + i * EDITOR_LINE_HEIGHT;
        window_draw_string(s_editor_win, text_start_x, row_y,
                           s_lines[line_idx], 0x00000000, 0x00FFFFFF, true);
    }

    /* 5. Draw caret cursor */
    if (s_cursor_row >= s_scroll_top && s_cursor_row < s_scroll_top + visible_lines) {
        int caret_x = text_start_x + s_cursor_col * FONT_GLYPH_WIDTH;
        int caret_y = text_start_y + (s_cursor_row - s_scroll_top) * EDITOR_LINE_HEIGHT;

        if (caret_x < EDITOR_CANVAS_X + canvas_w - 4) {
            window_fill_rect(s_editor_win, caret_x, caret_y, 2, FONT_GLYPH_HEIGHT, 0x00000000);
        }
    }

    /* Invalidate editor canvas region */
    wm_invalidate_rect(s_editor_win->client_x + EDITOR_CANVAS_X,
                       s_editor_win->client_y + EDITOR_CANVAS_Y,
                       canvas_w, canvas_h);

    /* Update metrics */
    update_status_bar();
}

/* Inserts character at current cursor position */
static void editor_insert_char(char c) {
    if (s_line_lens[s_cursor_row] >= EDITOR_MAX_COLS - 1) {
        return; /* Line full */
    }

    char* line = s_lines[s_cursor_row];
    int len = s_line_lens[s_cursor_row];

    /* Shift characters right */
    for (int i = len; i > s_cursor_col; i--) {
        line[i] = line[i - 1];
    }
    line[s_cursor_col] = c;
    s_cursor_col++;
    s_line_lens[s_cursor_row]++;
    line[s_line_lens[s_cursor_row]] = '\0';

    redraw_editor_canvas();
}

/* Inserts newline (splits current line at cursor) */
static void editor_insert_newline(void) {
    if (s_num_lines >= EDITOR_MAX_LINES - 1) {
        return; /* Max lines reached */
    }

    /* Shift lines down */
    for (int i = s_num_lines; i > s_cursor_row + 1; i--) {
        strcpy(s_lines[i], s_lines[i - 1]);
        s_line_lens[i] = s_line_lens[i - 1];
    }

    /* Move text after cursor on current line to new line */
    char* cur_line = s_lines[s_cursor_row];
    char* next_line = s_lines[s_cursor_row + 1];

    int move_len = s_line_lens[s_cursor_row] - s_cursor_col;
    for (int i = 0; i < move_len; i++) {
        next_line[i] = cur_line[s_cursor_col + i];
    }
    next_line[move_len] = '\0';
    s_line_lens[s_cursor_row + 1] = move_len;

    /* Truncate current line at cursor */
    cur_line[s_cursor_col] = '\0';
    s_line_lens[s_cursor_row] = s_cursor_col;

    s_cursor_row++;
    s_cursor_col = 0;
    s_num_lines++;

    redraw_editor_canvas();
}

/* Handles backspace deletion */
static void editor_handle_backspace(void) {
    char* cur_line = s_lines[s_cursor_row];

    if (s_cursor_col > 0) {
        /* Delete character before cursor */
        int len = s_line_lens[s_cursor_row];
        for (int i = s_cursor_col - 1; i < len - 1; i++) {
            cur_line[i] = cur_line[i + 1];
        }
        cur_line[len - 1] = '\0';
        s_line_lens[s_cursor_row]--;
        s_cursor_col--;
        redraw_editor_canvas();
    } else if (s_cursor_row > 0) {
        /* Join current line to previous line if fits */
        int prev_len = s_line_lens[s_cursor_row - 1];
        int cur_len = s_line_lens[s_cursor_row];

        if (prev_len + cur_len < EDITOR_MAX_COLS) {
            char* prev_line = s_lines[s_cursor_row - 1];
            for (int i = 0; i < cur_len; i++) {
                prev_line[prev_len + i] = cur_line[i];
            }
            prev_line[prev_len + cur_len] = '\0';
            s_line_lens[s_cursor_row - 1] += cur_len;

            /* Shift lines below up by 1 */
            for (int i = s_cursor_row; i < s_num_lines - 1; i++) {
                strcpy(s_lines[i], s_lines[i + 1]);
                s_line_lens[i] = s_line_lens[i + 1];
            }
            s_num_lines--;
            s_cursor_row--;
            s_cursor_col = prev_len;

            redraw_editor_canvas();
        }
    }
}

/* Window Key Event Dispatcher */
static void on_editor_key(window_t* win, char c) {
    (void)win;

    if (c >= 32 && c <= 126) {
        editor_insert_char(c);
    } else if (c == '\n' || c == '\r') {
        editor_insert_newline();
    } else if (c == '\b') {
        editor_handle_backspace();
    } else if (c == '\t') {
        /* Tab key inserts 4 spaces */
        for (int i = 0; i < 4; i++) {
            editor_insert_char(' ');
        }
    }
}

/* Clears buffer to a single blank line */
static void editor_clear_all(void) {
    for (int i = 0; i < EDITOR_MAX_LINES; i++) {
        s_lines[i][0] = '\0';
        s_line_lens[i] = 0;
    }
    s_num_lines = 1;
    s_cursor_row = 0;
    s_cursor_col = 0;
    s_scroll_top = 0;
}

/* Callback: New Document */
static void on_btn_new(widget_t* w, void* data) {
    (void)w; (void)data;
    editor_clear_all();
    strcpy(s_current_filepath, "/untitled.txt");
    if (s_editor_win) {
        strcpy(s_editor_win->title, "Text Editor - Untitled");
    }
    redraw_editor_canvas();
}

/* Callback: Save Document to VFS */
static void on_btn_save(widget_t* w, void* data) {
    (void)w; (void)data;

    int fd = vfs_open(s_current_filepath, 0x02);
    if (fd < 0) {
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, "Error: Failed to open file for saving!");
        }
        return;
    }

    /* Write lines separated by newlines */
    for (int i = 0; i < s_num_lines; i++) {
        if (s_line_lens[i] > 0) {
            vfs_write(fd, s_lines[i], (uint32_t)s_line_lens[i]);
        }
        if (i < s_num_lines - 1) {
            vfs_write(fd, "\n", 1);
        }
    }
    vfs_close(fd);

    char stat[80];
    strcpy(stat, "Saved successfully to ");
    strcat(stat, s_current_filepath);
    if (s_lbl_status) {
        widget_set_text(s_lbl_status, stat);
    }
    serial_writestring("[EDITOR] Document saved to VFS: ");
    serial_writestring(s_current_filepath);
    serial_writestring("\n");
}

/* Callback: Load Sample Document */
static void on_btn_sample(widget_t* w, void* data) {
    (void)w; (void)data;
    editor_clear_all();

    static const char* sample_text[] = {
        "Welcome to MyOS Text Editor!",
        "============================",
        "Features of this desktop editor:",
        "1. Multi-line editing buffer (64 lines)",
        "2. Dynamic cursor tracking and caret display",
        "3. Enter key line splitting and backspace join",
        "4. Status bar with real-time char metrics",
        "5. Clean integration with Z-order compositor",
        "6. Direct VFS save and load integration!",
        "",
        "Built from scratch in freestanding C!",
        "Type anything here to test..."
    };
    int num_samples = sizeof(sample_text) / sizeof(sample_text[0]);

    for (int i = 0; i < num_samples && i < EDITOR_MAX_LINES; i++) {
        strcpy(s_lines[i], sample_text[i]);
        s_line_lens[i] = (int)strlen(sample_text[i]);
    }
    s_num_lines = num_samples;
    s_cursor_row = num_samples - 1;
    s_cursor_col = s_line_lens[s_cursor_row];

    redraw_editor_canvas();
}

/* Window Close Handler */
static void on_editor_close(window_t* win) {
    wm_remove_window(win);
    s_editor_win = NULL;
    s_lbl_status = NULL;
}

/* Window resize handler */
static void on_editor_resize(window_t* win, int new_w, int new_h) {
    (void)new_w;
    (void)new_h;
    if (s_lbl_status && win) {
        s_lbl_status->y = win->client_height - 20;
        s_lbl_status->width = win->client_width - 24;
    }
    redraw_editor_canvas();
}

/*
 * app_editor_open:
 * Creates and displays the Text Editor application window.
 */
void app_editor_open(void) {
    if (s_editor_win) {
        wm_bring_to_front(s_editor_win);
        return;
    }

    s_editor_win = window_create(160, 80, 520, 360, "Text Editor - Untitled",
                                 WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_editor_win) return;

    s_editor_win->on_close  = on_editor_close;
    s_editor_win->on_key    = on_editor_key;
    s_editor_win->on_resize = on_editor_resize;

    /* 1. Toolbar Buttons */
    widget_add_to_window(s_editor_win, widget_create_button(10, 6, 75, 26, "New", on_btn_new, NULL));
    widget_add_to_window(s_editor_win, widget_create_button(92, 6, 75, 26, "Save", on_btn_save, NULL));
    widget_add_to_window(s_editor_win, widget_create_button(174, 6, 85, 26, "Sample", on_btn_sample, NULL));

    /* 2. Status Bar Label at bottom */
    s_lbl_status = widget_create_label(14, 316, 490, 18, "Ln 1, Col 1 | Lines: 1 | Chars: 0", 0x00303030);
    widget_add_to_window(s_editor_win, s_lbl_status);

    /* Initialize blank buffer */
    editor_clear_all();

    /* Render initial canvas and register window */
    redraw_editor_canvas();
    wm_add_window(s_editor_win);

    serial_writestring("[EDITOR] Text Editor window opened.\n");
}

/*
 * app_editor_open_file:
 * Opens the text editor and loads a document from VFS.
 */
void app_editor_open_file(const char* filepath) {
    app_editor_open();
    if (!s_editor_win || !filepath) return;

    editor_clear_all();
    strncpy(s_current_filepath, filepath, sizeof(s_current_filepath) - 1);

    /* Update title */
    char title_buf[WINDOW_TITLE_MAX];
    strcpy(title_buf, "Editor - ");
    const char* base = filepath;
    for (const char* p = filepath; *p; p++) {
        if (*p == '/') base = p + 1;
    }
    strncat(title_buf, base, WINDOW_TITLE_MAX - 10);
    strncpy(s_editor_win->title, title_buf, WINDOW_TITLE_MAX - 1);

    int fd = vfs_open(filepath, 0x01);
    if (fd < 0) {
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, "Could not open file");
        }
        redraw_editor_canvas();
        return;
    }

    uint32_t fsize = vfs_tell_size(fd);
    if (fsize > 0) {
        char* raw_buf = (char*)kmalloc(fsize + 1);
        if (raw_buf) {
            int n = vfs_read(fd, raw_buf, fsize);
            if (n >= 0) raw_buf[n] = '\0';

            /* Parse buffer line by line */
            int line = 0;
            int col = 0;
            for (int i = 0; i < n && line < EDITOR_MAX_LINES; i++) {
                char ch = raw_buf[i];
                if (ch == '\r') continue;
                if (ch == '\n') {
                    s_lines[line][col] = '\0';
                    s_line_lens[line] = col;
                    line++;
                    col = 0;
                } else {
                    if (col < EDITOR_MAX_COLS - 1) {
                        s_lines[line][col++] = ch;
                    }
                }
            }
            if (col > 0 && line < EDITOR_MAX_LINES) {
                s_lines[line][col] = '\0';
                s_line_lens[line] = col;
                line++;
            }
            s_num_lines = (line > 0) ? line : 1;
            kfree(raw_buf);
        }
    }
    vfs_close(fd);

    s_cursor_row = 0;
    s_cursor_col = 0;
    redraw_editor_canvas();
    update_status_bar();
}
