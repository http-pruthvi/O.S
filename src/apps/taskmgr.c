#include "apps/taskmgr.h"
#include "proc/task.h"
#include "proc/scheduler.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/* State Variables */
static window_t* s_taskmgr_win = NULL;
static uint32_t  s_selected_pid = 1; /* Default to PID 1 */
static widget_t* s_lbl_mem_stat = NULL;
static widget_t* s_lbl_status   = NULL;

#define TASKMGR_WIN_W 500
#define TASKMGR_WIN_H 350
#define TASKMGR_ROW_H 20
#define TASKMGR_MAX_ROWS 9

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

/* Returns human-readable string for task states */
static const char* state_to_str(task_state_t st) {
    switch (st) {
        case TASK_RUNNING:  return "RUNNING";
        case TASK_READY:    return "READY";
        case TASK_SLEEPING: return "SLEEP";
        case TASK_BLOCKED:  return "BLOCKED";
        case TASK_ZOMBIE:   return "ZOMBIE";
        case TASK_EMBRYO:   return "EMBRYO";
        default:            return "UNKNOWN";
    }
}

/* Repaints the process table inside the Task Manager client canvas */
static void taskmgr_draw_table(void) {
    if (!s_taskmgr_win) return;

    /* 1. Header Bar */
    window_fill_rect(s_taskmgr_win, 8, 8, TASKMGR_WIN_W - 16, 22, 0x001B365D);
    window_draw_string(s_taskmgr_win, 14, 11, "PID", 0x00FFFFFF, 0, true);
    window_draw_string(s_taskmgr_win, 60, 11, "Process Name", 0x00FFFFFF, 0, true);
    window_draw_string(s_taskmgr_win, 200, 11, "State", 0x00FFFFFF, 0, true);
    window_draw_string(s_taskmgr_win, 280, 11, "Ticks", 0x00FFFFFF, 0, true);
    window_draw_string(s_taskmgr_win, 360, 11, "Time Slice", 0x00FFFFFF, 0, true);

    /* 2. Process Rows Area */
    int y_start = 32;
    window_fill_rect(s_taskmgr_win, 8, y_start, TASKMGR_WIN_W - 16, TASKMGR_ROW_H * TASKMGR_MAX_ROWS, 0x00FFFFFF);
    window_draw_rect(s_taskmgr_win, 8, y_start, TASKMGR_WIN_W - 16, TASKMGR_ROW_H * TASKMGR_MAX_ROWS, 0x0090A0B0);

    task_t* curr = task_get_all();
    int row = 0;

    while (curr && row < TASKMGR_MAX_ROWS) {
        int ry = y_start + (row * TASKMGR_ROW_H);
        bool is_sel = (curr->id == s_selected_pid);

        uint32_t bg_col = is_sel ? 0x002B5797 : ((row % 2 == 0) ? 0x00FFFFFF : 0x00F2F5F8);
        uint32_t fg_col = is_sel ? 0x00FFFFFF : 0x00101820;

        window_fill_rect(s_taskmgr_win, 9, ry + 1, TASKMGR_WIN_W - 18, TASKMGR_ROW_H - 1, bg_col);

        char pid_str[16];
        fmt_uint(curr->id, pid_str);
        window_draw_string(s_taskmgr_win, 14, ry + 3, pid_str, fg_col, 0, true);

        window_draw_string(s_taskmgr_win, 60, ry + 3, curr->name, fg_col, 0, true);

        const char* st_str = state_to_str(curr->state);
        window_draw_string(s_taskmgr_win, 200, ry + 3, st_str, fg_col, 0, true);

        char ticks_str[16];
        fmt_uint(curr->ticks_run, ticks_str);
        window_draw_string(s_taskmgr_win, 280, ry + 3, ticks_str, fg_col, 0, true);

        char slice_str[16];
        fmt_uint(curr->time_slice, slice_str);
        window_draw_string(s_taskmgr_win, 360, ry + 3, slice_str, fg_col, 0, true);

        curr = curr->all_next;
        row++;
    }

    /* 3. Memory Statistics Panel */
    uint32_t free_mb = (pmm_free_frames() * 4) / 1024;
    size_t used_b = 0, free_b = 0, blocks = 0;
    heap_stats(&used_b, &free_b, &blocks);

    char mem_info[96];
    char f_str[16], u_str[16], b_str[16];
    fmt_uint(free_mb, f_str);
    fmt_uint((uint32_t)(used_b / 1024), u_str);
    fmt_uint((uint32_t)blocks, b_str);

    strcpy(mem_info, "RAM Free: ");
    strcat(mem_info, f_str);
    strcat(mem_info, " MB | Heap Used: ");
    strcat(mem_info, u_str);
    strcat(mem_info, " KB (Blocks: ");
    strcat(mem_info, b_str);
    strcat(mem_info, ")");

    if (s_lbl_mem_stat) {
        widget_set_text(s_lbl_mem_stat, mem_info);
    }
}

/* User clicks inside Task Manager window */
static void on_taskmgr_click(window_t* win, int rx, int ry) {
    (void)win;

    /* Check if click is inside table area (y: 32 .. 32 + TASKMGR_ROW_H * TASKMGR_MAX_ROWS) */
    if (rx >= 8 && rx <= TASKMGR_WIN_W - 8 && ry >= 32 && ry < (32 + TASKMGR_ROW_H * TASKMGR_MAX_ROWS)) {
        int clicked_row = (ry - 32) / TASKMGR_ROW_H;
        task_t* curr = task_get_all();
        int r = 0;
        while (curr && r < clicked_row) {
            curr = curr->all_next;
            r++;
        }
        if (curr) {
            s_selected_pid = curr->id;
            char msg[48];
            char p_buf[16];
            fmt_uint(curr->id, p_buf);
            strcpy(msg, "Selected Task: PID ");
            strcat(msg, p_buf);
            strcat(msg, " (");
            strcat(msg, curr->name);
            strcat(msg, ")");
            if (s_lbl_status) {
                widget_set_text(s_lbl_status, msg);
            }
            taskmgr_draw_table();
        }
    }
}

/* Button Callback: Refresh Process List */
static void on_btn_refresh(widget_t* w, void* data) {
    (void)w;
    (void)data;
    taskmgr_draw_table();
    if (s_lbl_status) {
        widget_set_text(s_lbl_status, "Process list refreshed");
    }
}

/* Button Callback: Kill Selected Task */
static void on_btn_end_task(widget_t* w, void* data) {
    (void)w;
    (void)data;

    if (s_selected_pid == 0 || s_selected_pid == 1) {
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, "Error: Cannot kill system kernel tasks!");
        }
        return;
    }

    if (task_kill(s_selected_pid)) {
        char msg[48];
        char p_buf[16];
        fmt_uint(s_selected_pid, p_buf);
        strcpy(msg, "Terminated Task PID ");
        strcat(msg, p_buf);
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, msg);
        }
        s_selected_pid = 1;
        taskmgr_draw_table();
    } else {
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, "Task not found or already exited");
        }
    }
}

/* Worker function spawned for dynamic testing */
static void dummy_worker(void* arg) {
    (void)arg;
    for (int i = 0; i < 60; i++) {
        task_sleep_ms(500);
    }
}

/* Button Callback: Spawn a new worker task */
static void on_btn_spawn_task(widget_t* w, void* data) {
    (void)w;
    (void)data;

    task_t* t = task_create("worker_task", dummy_worker, NULL);
    if (t) {
        s_selected_pid = t->id;
        char msg[48];
        char p_buf[16];
        fmt_uint(t->id, p_buf);
        strcpy(msg, "Spawned Task PID ");
        strcat(msg, p_buf);
        if (s_lbl_status) {
            widget_set_text(s_lbl_status, msg);
        }
        taskmgr_draw_table();
    }
}

/* Window close callback */
static void on_taskmgr_close(window_t* win) {
    wm_remove_window(win);
    s_taskmgr_win   = NULL;
    s_lbl_mem_stat  = NULL;
    s_lbl_status    = NULL;
}

/*
 * app_taskmgr_open:
 * Creates and displays the Task Manager application window.
 */
void app_taskmgr_open(void) {
    if (s_taskmgr_win) {
        wm_bring_to_front(s_taskmgr_win);
        taskmgr_draw_table();
        return;
    }

    s_taskmgr_win = window_create(150, 70, TASKMGR_WIN_W, TASKMGR_WIN_H,
                                  "Task Manager",
                                  WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_taskmgr_win) return;

    s_taskmgr_win->on_close = on_taskmgr_close;
    s_taskmgr_win->on_click = on_taskmgr_click;

    /* Memory stats label */
    s_lbl_mem_stat = widget_create_label(12, 222, TASKMGR_WIN_W - 24, 18, "", 0x00204060);
    widget_add_to_window(s_taskmgr_win, s_lbl_mem_stat);

    /* Status label */
    s_lbl_status = widget_create_label(12, 244, TASKMGR_WIN_W - 24, 18, "Click a row to select task", 0x00607080);
    widget_add_to_window(s_taskmgr_win, s_lbl_status);

    /* Control Buttons */
    widget_add_to_window(s_taskmgr_win,
        widget_create_button(12, 274, 90, 32, "Refresh", on_btn_refresh, NULL));

    widget_add_to_window(s_taskmgr_win,
        widget_create_button(112, 274, 105, 32, "End Task", on_btn_end_task, NULL));

    widget_add_to_window(s_taskmgr_win,
        widget_create_button(227, 274, 115, 32, "+ Spawn Task", on_btn_spawn_task, NULL));

    wm_add_window(s_taskmgr_win);
    taskmgr_draw_table();
    serial_writestring("[TASKMGR] Task Manager window opened\n");
}
