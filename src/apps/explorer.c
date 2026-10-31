#include "apps/explorer.h"
#include "apps/editor.h"
#include "fs/vfs.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define EXPLORER_WIN_W       520
#define EXPLORER_WIN_H       360
#define EXPLORER_ROW_H       22
#define EXPLORER_MAX_ITEMS   10

static window_t* s_explorer_win = NULL;
static int       s_selected_idx = 0;
static widget_t* s_lbl_status   = NULL;

/* Cached list of directory entries */
static vfs_dirent_t s_cached_entries[EXPLORER_MAX_ITEMS];
static int          s_item_count = 0;

/* Helper: formats uint32_t into decimal string */
static void fmt_uint(uint32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char tmp[16];
    int t = 0;
    while (val > 0) {
        tmp[t++] = (char)('0' + (val % 10));
        val /= 10;
    }
    int b = 0;
    for (int i = t - 1; i >= 0; i--) {
        buf[b++] = tmp[i];
    }
    buf[b] = '\0';
}

/* Scans root directory and refreshes cache */
static void explorer_scan_directory(void) {
    s_item_count = 0;
    vfs_node_t* root = vfs_get_root();
    if (!root || !root->readdir) return;

    for (uint32_t i = 0; i < EXPLORER_MAX_ITEMS; i++) {
        vfs_dirent_t* d = root->readdir(root, i);
        if (!d) break;
        s_cached_entries[s_item_count++] = *d;
    }

    if (s_selected_idx >= s_item_count) {
        s_selected_idx = (s_item_count > 0) ? (s_item_count - 1) : 0;
    }
}

/* Redraws table of files inside File Explorer client canvas */
static void explorer_draw_list(void) {
    if (!s_explorer_win) return;

    /* 1. Header Bar */
    window_fill_rect(s_explorer_win, 8, 44, EXPLORER_WIN_W - 16, 22, 0x00243447);
    window_draw_string(s_explorer_win, 14, 47, "Type", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, 70, 47, "File Name", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, 270, 47, "Size", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, 370, 47, "Permissions", 0x00FFFFFF, 0, true);

    /* 2. File Items Area */
    int y_start = 68;
    int list_h = EXPLORER_ROW_H * EXPLORER_MAX_ITEMS;
    window_fill_rect(s_explorer_win, 8, y_start, EXPLORER_WIN_W - 16, list_h, 0x00FFFFFF);
    window_draw_rect(s_explorer_win, 8, y_start, EXPLORER_WIN_W - 16, list_h, 0x00A0B0C0);

    for (int i = 0; i < s_item_count; i++) {
        int ry = y_start + (i * EXPLORER_ROW_H);
        bool is_sel = (i == s_selected_idx);

        uint32_t bg_col = is_sel ? 0x002B5797 : ((i % 2 == 0) ? 0x00FFFFFF : 0x00F5F7FA);
        uint32_t fg_col = is_sel ? 0x00FFFFFF : 0x00101820;

        window_fill_rect(s_explorer_win, 9, ry + 1, EXPLORER_WIN_W - 18, EXPLORER_ROW_H - 1, bg_col);

        /* Type icon */
        const char* type_str = (s_cached_entries[i].type == VFS_DIRECTORY) ? "[DIR]" : "[TXT]";
        window_draw_string(s_explorer_win, 14, ry + 3, type_str, is_sel ? 0x00FFFFFF : 0x000066CC, 0, true);

        /* File Name */
        window_draw_string(s_explorer_win, 70, ry + 3, s_cached_entries[i].name, fg_col, 0, true);

        /* Size in bytes */
        char size_buf[24];
        fmt_uint(s_cached_entries[i].size, size_buf);
        strcat(size_buf, " B");
        window_draw_string(s_explorer_win, 270, ry + 3, size_buf, fg_col, 0, true);

        /* Permissions */
        window_draw_string(s_explorer_win, 370, ry + 3, "rw-r--r--", fg_col, 0, true);
    }

    /* 3. Update status text */
    if (s_lbl_status) {
        char stat_buf[64];
        char count_buf[16];
        fmt_uint((uint32_t)s_item_count, count_buf);
        strcpy(stat_buf, "Total: ");
        strcat(stat_buf, count_buf);
        strcat(stat_buf, " item(s) in / | RamFS Active");
        widget_set_text(s_lbl_status, stat_buf);
    }
}

/* Handles clicks inside Explorer window */
static void on_explorer_click(window_t* win, int rx, int ry) {
    (void)win;

    /* Check if click is inside file list */
    if (rx >= 8 && rx <= EXPLORER_WIN_W - 8 && ry >= 68 && ry < (68 + EXPLORER_ROW_H * EXPLORER_MAX_ITEMS)) {
        int clicked_idx = (ry - 68) / EXPLORER_ROW_H;
        if (clicked_idx < s_item_count) {
            s_selected_idx = clicked_idx;
            explorer_draw_list();
        }
    }
}

/* Button Callback: Open selected file in Editor */
static void on_btn_open_file(widget_t* w, void* data) {
    (void)w;
    (void)data;

    if (s_item_count == 0 || s_selected_idx < 0 || s_selected_idx >= s_item_count) {
        return;
    }

    char fullpath[VFS_NAME_MAX + 2];
    fullpath[0] = '/';
    fullpath[1] = '\0';
    strcat(fullpath, s_cached_entries[s_selected_idx].name);

    app_editor_open_file(fullpath);
}

/* Button Callback: Refresh file list */
static void on_btn_refresh(widget_t* w, void* data) {
    (void)w;
    (void)data;

    explorer_scan_directory();
    explorer_draw_list();
}

/* Button Callback: Create a new note file */
static void on_btn_new_file(widget_t* w, void* data) {
    (void)w;
    (void)data;

    /* Generate unique name */
    static int s_new_counter = 1;
    char fname[32] = "note_";
    char num_str[16];
    fmt_uint((uint32_t)s_new_counter++, num_str);
    strcat(fname, num_str);
    strcat(fname, ".txt");

    char fpath[36] = "/";
    strcat(fpath, fname);

    int fd = vfs_open(fpath, 0x02);
    if (fd >= 0) {
        const char* default_text = "New note created from File Explorer.\nEdit your thoughts here!\n";
        vfs_write(fd, default_text, strlen(default_text));
        vfs_close(fd);
    }

    explorer_scan_directory();
    /* Select newly created item */
    for (int i = 0; i < s_item_count; i++) {
        if (strcmp(s_cached_entries[i].name, fname) == 0) {
            s_selected_idx = i;
            break;
        }
    }
    explorer_draw_list();
}

/* Button Callback: Delete selected file */
static void on_btn_delete_file(widget_t* w, void* data) {
    (void)w;
    (void)data;

    if (s_item_count == 0 || s_selected_idx < 0 || s_selected_idx >= s_item_count) {
        return;
    }

    char fpath[VFS_NAME_MAX + 2] = "/";
    strcat(fpath, s_cached_entries[s_selected_idx].name);

    vfs_delete(fpath);

    explorer_scan_directory();
    explorer_draw_list();
}

/* Window close callback */
static void on_explorer_close(window_t* win) {
    wm_remove_window(win);
    s_explorer_win = NULL;
    s_lbl_status   = NULL;
}

/*
 * app_explorer_open:
 * Creates and displays the File Explorer application window.
 */
void app_explorer_open(void) {
    if (s_explorer_win) {
        wm_bring_to_front(s_explorer_win);
        explorer_scan_directory();
        explorer_draw_list();
        return;
    }

    s_explorer_win = window_create(140, 60, EXPLORER_WIN_W, EXPLORER_WIN_H,
                                   "File Explorer - /",
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_explorer_win) return;

    s_explorer_win->on_close = on_explorer_close;
    s_explorer_win->on_click = on_explorer_click;

    /* 1. Location Bar */
    window_draw_string(s_explorer_win, 12, 14, "Path: /", 0x00102030, 0, true);

    /* 2. Action Toolbar Buttons */
    widget_add_to_window(s_explorer_win,
        widget_create_button(120, 8, 85, 28, "Open", on_btn_open_file, NULL));

    widget_add_to_window(s_explorer_win,
        widget_create_button(215, 8, 95, 28, "+ New File", on_btn_new_file, NULL));

    widget_add_to_window(s_explorer_win,
        widget_create_button(320, 8, 85, 28, "Delete", on_btn_delete_file, NULL));

    widget_add_to_window(s_explorer_win,
        widget_create_button(415, 8, 90, 28, "Refresh", on_btn_refresh, NULL));

    /* 3. Status Bar */
    s_lbl_status = widget_create_label(12, 322, EXPLORER_WIN_W - 24, 18, "", 0x00506070);
    widget_add_to_window(s_explorer_win, s_lbl_status);

    wm_add_window(s_explorer_win);
    explorer_scan_directory();
    explorer_draw_list();

    serial_writestring("[EXPLORER] File Explorer window opened\n");
}

