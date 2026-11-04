#include "apps/explorer.h"
#include "apps/editor.h"
#include "apps/store.h"
#include "fs/vfs.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define EXPLORER_WIN_W       520
#define EXPLORER_WIN_H       360
#define EXPLORER_ROW_H       22
#define EXPLORER_MAX_ITEMS   16

#define CONTEXT_MENU_W       210
#define CONTEXT_MENU_H       146
#define CONTEXT_ITEM_H       26

static window_t* s_explorer_win = NULL;
static int       s_selected_idx = 0;
static widget_t* s_lbl_status   = NULL;

/* Cached list of directory entries */
static vfs_dirent_t s_cached_entries[EXPLORER_MAX_ITEMS];
static int          s_item_count = 0;

/* Context Menu state */
static bool s_menu_open       = false;
static int  s_menu_x          = 0;
static int  s_menu_y          = 0;
static int  s_menu_target_idx = -1;

/* File Properties Dialog Window */
static window_t* s_prop_win   = NULL;

/* Forward declarations */
static void explorer_scan_directory(void);
static void explorer_draw_list(void);
static void explorer_duplicate_file(int idx);
static void explorer_show_properties(int idx);

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

    int cur_w = s_explorer_win->client_width;
    int cur_h = s_explorer_win->client_height;

    /* 1. Header Bar */
    window_fill_rect(s_explorer_win, 8, 44, cur_w - 16, 22, 0x00243447);
    window_draw_string(s_explorer_win, 14, 47, "Type", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, 70, 47, "File Name", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, cur_w - 240, 47, "Size", 0x00FFFFFF, 0, true);
    window_draw_string(s_explorer_win, cur_w - 140, 47, "Permissions", 0x00FFFFFF, 0, true);

    /* 2. File Items Area */
    int y_start = 68;
    int avail_h = cur_h - 68 - 28;
    int max_rows = avail_h / EXPLORER_ROW_H;
    if (max_rows < 1) max_rows = 1;
    int list_h = EXPLORER_ROW_H * max_rows;

    window_fill_rect(s_explorer_win, 8, y_start, cur_w - 16, list_h, 0x00FFFFFF);
    window_draw_rect(s_explorer_win, 8, y_start, cur_w - 16, list_h, 0x00A0B0C0);

    int rows_to_draw = (s_item_count < max_rows) ? s_item_count : max_rows;

    for (int i = 0; i < rows_to_draw; i++) {
        int ry = y_start + (i * EXPLORER_ROW_H);
        bool is_sel = (i == s_selected_idx);

        uint32_t bg_col = is_sel ? 0x002B5797 : ((i % 2 == 0) ? 0x00FFFFFF : 0x00F5F7FA);
        uint32_t fg_col = is_sel ? 0x00FFFFFF : 0x00101820;

        window_fill_rect(s_explorer_win, 9, ry + 1, cur_w - 18, EXPLORER_ROW_H - 1, bg_col);

        /* Type icon */
        const char* type_str = (s_cached_entries[i].type == VFS_DIRECTORY) ? "[DIR]" : "[TXT]";
        window_draw_string(s_explorer_win, 14, ry + 3, type_str, is_sel ? 0x00FFFFFF : 0x000066CC, 0, true);

        /* File Name */
        window_draw_string(s_explorer_win, 70, ry + 3, s_cached_entries[i].name, fg_col, 0, true);

        /* Size in bytes */
        char size_buf[24];
        fmt_uint(s_cached_entries[i].size, size_buf);
        strcat(size_buf, " B");
        window_draw_string(s_explorer_win, cur_w - 240, ry + 3, size_buf, fg_col, 0, true);

        /* Permissions */
        window_draw_string(s_explorer_win, cur_w - 140, ry + 3, "rw-r--r--", fg_col, 0, true);
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

    wm_invalidate_rect(s_explorer_win->client_x + 8, s_explorer_win->client_y + 44,
                       cur_w - 16, cur_h - 44);
}

/* Handles left clicks inside Explorer window */
static void on_explorer_click(window_t* win, int rx, int ry) {
    (void)win;

    /* Dismiss any active context menu when clicking in window */
    if (s_menu_open) {
        explorer_context_menu_dismiss();
    }

    /* Check if click is inside file list */
    if (rx >= 8 && rx <= s_explorer_win->client_width - 8 && ry >= 68) {
        int clicked_idx = (ry - 68) / EXPLORER_ROW_H;
        if (clicked_idx >= 0 && clicked_idx < s_item_count) {
            s_selected_idx = clicked_idx;
            explorer_draw_list();
        }
    }
}

/* Handles right clicks inside Explorer window to open Fluent Context Menu */
static void on_explorer_right_click(window_t* win, int rx, int ry) {
    /* Check if right click occurred on a file row */
    if (rx >= 8 && rx <= win->client_width - 8 && ry >= 68) {
        int clicked_idx = (ry - 68) / EXPLORER_ROW_H;
        if (clicked_idx >= 0 && clicked_idx < s_item_count) {
            s_selected_idx = clicked_idx;
            s_menu_target_idx = clicked_idx;
            s_menu_open = true;

            /* Compute screen coordinates for context menu */
            int sx = win->client_x + rx;
            int sy = win->client_y + ry;

            /* Boundary clamp to ensure menu remains fully visible */
            if (sx + CONTEXT_MENU_W > WM_SCREEN_WIDTH - 10) {
                sx = WM_SCREEN_WIDTH - CONTEXT_MENU_W - 10;
            }
            if (sy + CONTEXT_MENU_H > WM_TASKBAR_Y - 10) {
                sy = WM_TASKBAR_Y - CONTEXT_MENU_H - 10;
            }
            if (sx < 10) sx = 10;
            if (sy < 10) sy = 10;

            s_menu_x = sx;
            s_menu_y = sy;

            explorer_draw_list();
            wm_redraw_all();
            return;
        }
    }

    /* Right click elsewhere dismisses context menu */
    explorer_context_menu_dismiss();
}

/* Window resize callback */
static void on_explorer_resize(window_t* win, int new_w, int new_h) {
    (void)new_w;
    (void)new_h;
    if (s_lbl_status && win) {
        s_lbl_status->y = win->client_height - 22;
        s_lbl_status->width = win->client_width - 24;
    }
    explorer_draw_list();
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
    explorer_context_menu_dismiss();
    if (s_prop_win) {
        wm_remove_window(s_prop_win);
        s_prop_win = NULL;
    }
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
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_explorer_win) return;

    s_explorer_win->on_close       = on_explorer_close;
    s_explorer_win->on_click       = on_explorer_click;
    s_explorer_win->on_right_click = on_explorer_right_click;
    s_explorer_win->on_resize      = on_explorer_resize;

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
    s_lbl_status = widget_create_label(12, EXPLORER_WIN_H - 38, EXPLORER_WIN_W - 24, 18, "", 0x00506070);
    widget_add_to_window(s_explorer_win, s_lbl_status);

    wm_add_window(s_explorer_win);
    explorer_scan_directory();
    explorer_draw_list();

    serial_writestring("[EXPLORER] File Explorer window opened (Resizable & Fluent Context Menu enabled)\n");
}

/*
 * Duplicate File operation: Creates <filename>.copy in RamFS
 */
static void explorer_duplicate_file(int idx) {
    if (idx < 0 || idx >= s_item_count) return;

    char src_path[VFS_NAME_MAX + 2] = "/";
    strcat(src_path, s_cached_entries[idx].name);

    char dst_name[VFS_NAME_MAX + 8];
    strcpy(dst_name, s_cached_entries[idx].name);
    strcat(dst_name, ".copy");

    char dst_path[VFS_NAME_MAX + 10] = "/";
    strcat(dst_path, dst_name);

    int fd_in = vfs_open(src_path, 0x01);
    if (fd_in < 0) return;

    uint8_t copy_buf[2048];
    int bytes = vfs_read(fd_in, copy_buf, sizeof(copy_buf));
    vfs_close(fd_in);

    int fd_out = vfs_open(dst_path, 0x02);
    if (fd_out >= 0) {
        if (bytes > 0) {
            vfs_write(fd_out, copy_buf, (uint32_t)bytes);
        }
        vfs_close(fd_out);
    }

    explorer_scan_directory();
    /* Focus the duplicated file */
    for (int i = 0; i < s_item_count; i++) {
        if (strcmp(s_cached_entries[i].name, dst_name) == 0) {
            s_selected_idx = i;
            break;
        }
    }
    explorer_draw_list();
}

/*
 * File Properties Dialog callbacks
 */
static void on_prop_close(window_t* win) {
    wm_remove_window(win);
    s_prop_win = NULL;
}

static void on_btn_prop_ok(widget_t* w, void* data) {
    (void)w;
    (void)data;
    if (s_prop_win) {
        wm_remove_window(s_prop_win);
        s_prop_win = NULL;
    }
}

/*
 * Displays modern File Properties popup dialog
 */
static void explorer_show_properties(int idx) {
    if (idx < 0 || idx >= s_item_count) return;

    if (s_prop_win) {
        wm_remove_window(s_prop_win);
        s_prop_win = NULL;
    }

    char fpath[VFS_NAME_MAX + 2] = "/";
    strcat(fpath, s_cached_entries[idx].name);

    vfs_node_t* node = vfs_lookup(fpath);

    char title[64] = "Properties: ";
    strncat(title, s_cached_entries[idx].name, 38);

    s_prop_win = window_create(320, 190, 360, 260, title,
                               WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_prop_win) return;

    s_prop_win->on_close = on_prop_close;

    /* Dialog Body */
    window_fill_rect(s_prop_win, 10, 10, 340, 185, 0x00FFFFFF);
    window_draw_rect(s_prop_win, 10, 10, 340, 185, 0x00CBD5E1);

    /* Header Icon and Name */
    bool is_dir = (s_cached_entries[idx].type == VFS_DIRECTORY);
    uint32_t icon_bg = is_dir ? 0x00F59E0B : 0x003B82F6;
    window_fill_rect(s_prop_win, 20, 20, 38, 36, icon_bg);
    window_draw_string(s_prop_win, 23, 30, is_dir ? "[DIR]" : "[TXT]", 0x00FFFFFF, icon_bg, true);
    window_draw_string(s_prop_win, 68, 28, s_cached_entries[idx].name, 0x000F172A, 0x00FFFFFF, true);

    window_fill_rect(s_prop_win, 20, 64, 320, 1, 0x00E2E8F0);

    /* Type */
    window_draw_string(s_prop_win, 20, 74, "Type: ", 0x0064748B, 0x00FFFFFF, true);
    const char* type_desc = is_dir ? "Directory Folder" : "Plain Text Document (.txt)";
    window_draw_string(s_prop_win, 110, 74, type_desc, 0x000F172A, 0x00FFFFFF, true);

    /* Location */
    window_draw_string(s_prop_win, 20, 96, "Location: ", 0x0064748B, 0x00FFFFFF, true);
    window_draw_string(s_prop_win, 110, 96, fpath, 0x000F172A, 0x00FFFFFF, true);

    /* Size */
    char size_buf[32];
    fmt_uint(s_cached_entries[idx].size, size_buf);
    strcat(size_buf, " bytes");
    window_draw_string(s_prop_win, 20, 118, "Size: ", 0x0064748B, 0x00FFFFFF, true);
    window_draw_string(s_prop_win, 110, 118, size_buf, 0x000F172A, 0x00FFFFFF, true);

    /* RamFS Inode Address */
    char addr_buf[32] = "0x";
    uint32_t node_addr = node ? (uint32_t)node : 0;
    static const char hex_digits[] = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        uint8_t nibble = (node_addr >> (i * 4)) & 0x0F;
        int len = strlen(addr_buf);
        addr_buf[len] = hex_digits[nibble];
        addr_buf[len + 1] = '\0';
    }
    window_draw_string(s_prop_win, 20, 140, "RamFS Node: ", 0x0064748B, 0x00FFFFFF, true);
    window_draw_string(s_prop_win, 110, 140, addr_buf, 0x002563EB, 0x00FFFFFF, true);

    /* Permissions */
    window_draw_string(s_prop_win, 20, 162, "Permissions: ", 0x0064748B, 0x00FFFFFF, true);
    window_draw_string(s_prop_win, 110, 162, "rw-r--r-- (0644)", 0x000F172A, 0x00FFFFFF, true);

    /* OK Button */
    widget_t* btn_ok = widget_create_button(135, 205, 90, 26, "OK", on_btn_prop_ok, NULL);
    widget_add_to_window(s_prop_win, btn_ok);

    wm_add_window(s_prop_win);
}

/*
 * Checks if context menu is active
 */
bool explorer_context_menu_is_open(void) {
    return s_menu_open;
}

/*
 * Dismisses context menu
 */
void explorer_context_menu_dismiss(void) {
    if (s_menu_open) {
        s_menu_open = false;
        s_menu_target_idx = -1;
        wm_redraw_all();
    }
}

/*
 * Composites modern Windows 11 Fluent context menu onto screen back buffer
 */
void explorer_context_menu_render(uint32_t* backbuf, int screen_w, int screen_h) {
    if (!s_menu_open || !backbuf) return;

    /* 1. Drop shadow: 3px semi-transparent shadow */
    for (int r = s_menu_y + 3; r < s_menu_y + CONTEXT_MENU_H + 3; r++) {
        if (r < 0 || r >= screen_h) continue;
        uint32_t* line = &backbuf[r * screen_w];
        for (int c = s_menu_x + 3; c < s_menu_x + CONTEXT_MENU_W + 3; c++) {
            if (c < 0 || c >= screen_w) continue;
            uint32_t p = line[c];
            uint32_t dr = ((p >> 16) & 0xFF) / 2;
            uint32_t dg = ((p >> 8) & 0xFF) / 2;
            uint32_t db = (p & 0xFF) / 2;
            line[c] = (dr << 16) | (dg << 8) | db;
        }
    }

    /* 2. Menu Background & 1px Crisp Border */
    uint32_t bg_col = 0x00222222;
    uint32_t bd_col = 0x003D3D3D;
    for (int r = s_menu_y; r < s_menu_y + CONTEXT_MENU_H; r++) {
        if (r < 0 || r >= screen_h) continue;
        uint32_t* line = &backbuf[r * screen_w];
        for (int c = s_menu_x; c < s_menu_x + CONTEXT_MENU_W; c++) {
            if (c < 0 || c >= screen_w) continue;
            if (r == s_menu_y || r == s_menu_y + CONTEXT_MENU_H - 1 ||
                c == s_menu_x || c == s_menu_x + CONTEXT_MENU_W - 1) {
                line[c] = bd_col;
            } else {
                line[c] = bg_col;
            }
        }
    }

    /* 3. Items and Badges */
    typedef struct {
        const char* tag;
        uint32_t    tag_col;
        const char* label;
    } menu_item_info_t;

    static const menu_item_info_t items[5] = {
        { "[TXT]",  0x0060A5FA, "Open in Text Editor" },
        { "[HEX]",  0x0038BDF8, "Inspect in Hex Pro" },
        { "[DUP]",  0x004ADE80, "Duplicate File" },
        { "[DEL]",  0x00F87171, "Delete File" },
        { "[INFO]", 0x00C084FC, "File Properties" }
    };

    int item_y = s_menu_y + 8;
    for (int i = 0; i < 5; i++) {
        /* Divider line before Delete (i=3) and Properties (i=4) */
        if (i == 3 || i == 4) {
            int div_y = item_y - 2;
            if (div_y >= 0 && div_y < screen_h) {
                uint32_t* line = &backbuf[div_y * screen_w];
                for (int c = s_menu_x + 8; c < s_menu_x + CONTEXT_MENU_W - 8; c++) {
                    if (c >= 0 && c < screen_w) {
                        line[c] = 0x00333333;
                    }
                }
            }
        }

        /* Draw Badge and Text */
        gfx_draw_string(s_menu_x + 10, item_y + 4, items[i].tag, items[i].tag_col, bg_col, true);
        gfx_draw_string(s_menu_x + 58, item_y + 4, items[i].label, 0x00F0F0F0, bg_col, true);

        item_y += CONTEXT_ITEM_H;
    }
}

/*
 * Handles clicks targeting context menu
 */
bool explorer_context_menu_handle_click(int mx, int my, bool is_right_click) {
    (void)is_right_click;
    if (!s_menu_open) return false;

    if (mx >= s_menu_x && mx < s_menu_x + CONTEXT_MENU_W &&
        my >= s_menu_y && my < s_menu_y + CONTEXT_MENU_H) {
        int clicked_item = (my - (s_menu_y + 8)) / CONTEXT_ITEM_H;
        if (clicked_item < 0) clicked_item = 0;
        if (clicked_item > 4) clicked_item = 4;

        int target = s_menu_target_idx;
        s_menu_open = false;
        s_menu_target_idx = -1;

        if (target >= 0 && target < s_item_count) {
            char filepath[VFS_NAME_MAX + 2];
            filepath[0] = '/';
            filepath[1] = '\0';
            strcat(filepath, s_cached_entries[target].name);

            switch (clicked_item) {
                case 0: /* Open in Text Editor */
                    app_editor_open_file(filepath);
                    break;
                case 1: /* Inspect in Hex Pro */
                    launch_hex_editor_file(filepath);
                    break;
                case 2: /* Duplicate File */
                    explorer_duplicate_file(target);
                    break;
                case 3: /* Delete File */
                    vfs_delete(filepath);
                    explorer_scan_directory();
                    explorer_draw_list();
                    break;
                case 4: /* File Properties */
                    explorer_show_properties(target);
                    break;
            }
        }
        wm_redraw_all();
        return true;
    }

    /* Click was outside: dismiss menu */
    s_menu_open = false;
    s_menu_target_idx = -1;
    wm_redraw_all();
    return false;
}
