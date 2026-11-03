#include "apps/terminal.h"
#include "apps/calc.h"
#include "apps/editor.h"
#include "apps/explorer.h"
#include "apps/taskmgr.h"
#include "apps/minesweeper.h"
#include "apps/snake.h"
#include "apps/control.h"
#include "apps/store.h"
#include "gfx/gfx.h"
#include "fs/vfs.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "proc/task.h"
#include "proc/scheduler.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define TERM_WIN_W    560
#define TERM_WIN_H    360
#define TERM_COLS     65
#define TERM_ROWS     18
#define TERM_CHAR_W   8
#define TERM_CHAR_H   16
#define TERM_PAD_X    8
#define TERM_PAD_Y    8

static window_t* s_term_win = NULL;

static char     s_grid[TERM_ROWS][TERM_COLS + 1];
static uint32_t s_fg_colors[TERM_ROWS][TERM_COLS];
static int      s_cur_row = 0;
static int      s_cur_col = 0;

static char     s_cmd_buf[128];
static int      s_cmd_len = 0;

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

/* Redraws the terminal grid into the window client canvas */
static void term_redraw(void) {
    if (!s_term_win) return;

    window_clear_client(s_term_win, 0x000F141C);

    for (int r = 0; r < TERM_ROWS; r++) {
        int y = TERM_PAD_Y + (r * TERM_CHAR_H);
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = s_grid[r][c];
            if (ch != '\0' && ch != ' ') {
                char str[2] = { ch, '\0' };
                int x = TERM_PAD_X + (c * TERM_CHAR_W);
                window_draw_string(s_term_win, x, y, str, s_fg_colors[r][c], 0x000F141C, true);
            }
        }
    }

    /* Draw blinking caret cursor at current insertion point */
    int cx = TERM_PAD_X + (s_cur_col * TERM_CHAR_W);
    int cy = TERM_PAD_Y + (s_cur_row * TERM_CHAR_H);
    window_fill_rect(s_term_win, cx, cy + 13, 8, 3, 0x0000FF99);
}

/* Scrolls terminal up by one line */
static void term_scroll_up(void) {
    for (int r = 0; r < TERM_ROWS - 1; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            s_grid[r][c] = s_grid[r + 1][c];
            s_fg_colors[r][c] = s_fg_colors[r + 1][c];
        }
        s_grid[r][TERM_COLS] = '\0';
    }

    for (int c = 0; c < TERM_COLS; c++) {
        s_grid[TERM_ROWS - 1][c] = ' ';
        s_fg_colors[TERM_ROWS - 1][c] = 0x00E0E0E0;
    }
    s_grid[TERM_ROWS - 1][TERM_COLS] = '\0';
    s_cur_row = TERM_ROWS - 1;
}

/* Emits a character into terminal grid */
static void term_putchar_color(char ch, uint32_t color) {
    if (ch == '\n') {
        s_cur_col = 0;
        s_cur_row++;
        if (s_cur_row >= TERM_ROWS) {
            term_scroll_up();
        }
        return;
    }

    if (ch == '\r') {
        s_cur_col = 0;
        return;
    }

    if (ch == '\b') {
        if (s_cur_col > 0) {
            s_cur_col--;
            s_grid[s_cur_row][s_cur_col] = ' ';
        }
        return;
    }

    if (s_cur_col >= TERM_COLS) {
        s_cur_col = 0;
        s_cur_row++;
        if (s_cur_row >= TERM_ROWS) {
            term_scroll_up();
        }
    }

    s_grid[s_cur_row][s_cur_col] = ch;
    s_fg_colors[s_cur_row][s_cur_col] = color;
    s_cur_col++;
}

/* Prints a string to terminal */
static void term_print(const char* str, uint32_t color) {
    if (!str) return;
    while (*str) {
        term_putchar_color(*str++, color);
    }
}

/* Clears terminal */
static void term_clear(void) {
    for (int r = 0; r < TERM_ROWS; r++) {
        for (int c = 0; c < TERM_COLS; c++) {
            s_grid[r][c] = ' ';
            s_fg_colors[r][c] = 0x00E0E0E0;
        }
        s_grid[r][TERM_COLS] = '\0';
    }
    s_cur_row = 0;
    s_cur_col = 0;
}

/* Displays prompt */
static void term_prompt(void) {
    term_print("myos:/$ ", 0x0000FF99);
}

/*
 * term_execute_self_destruct:
 * Executes emergency self-destruct protocol triggered by 'http_pruthvi command code 0'.
 *
 * Why this is needed:
 * Implements an authorized security wipe protocol that purges all in-memory volatile
 * filesystem data (RamFS nodes, caches, and user files) inside the running kernel while
 * leaving the host Git repository completely safe and intact.
 */
static void term_execute_self_destruct(void) {
    serial_writestring("\n========================================================\n");
    serial_writestring("[SECURITY] EMERGENCY SELF-DESTRUCT CODE ACTIVATED!\n");
    serial_writestring("COMMAND: 'http_pruthvi command code 0'\n");
    serial_writestring("========================================================\n");

    term_clear();
    term_print("=========================================================\n", 0x00FF3333);
    term_print(" [!] EMERGENCY SELF-DESTRUCT SEQUENCE INITIATED\n", 0x00FFFF00);
    term_print(" Operator Code: http_pruthvi command code 0\n", 0x00FFFFFF);
    term_print(" Authorization: OVERRIDE_LEVEL_ALPHA (ROOT)\n", 0x00FF8040);
    term_print(" Target: IN-MEMORY VOLATILE STORAGE & USER STATE\n", 0x00FF3333);
    term_print("=========================================================\n\n", 0x00FF3333);

    /* 1. Enumerate and delete all files in RamFS */
    vfs_node_t* root = vfs_get_root();
    int count = 0;
    if (root && root->readdir) {
        char names[32][VFS_NAME_MAX];
        int num_files = 0;
        for (uint32_t i = 0; i < 32; i++) {
            vfs_dirent_t* d = root->readdir(root, i);
            if (!d) break;
            if (d->type == VFS_FILE) {
                strlcpy(names[num_files++], d->name, VFS_NAME_MAX);
            }
        }
        for (int i = 0; i < num_files; i++) {
            char path[VFS_NAME_MAX + 2];
            path[0] = '/';
            path[1] = '\0';
            strlcat(path, names[i], sizeof(path));
            vfs_delete(path);
            term_print("[WIPE] Purged: ", 0x00FF5555);
            term_print(path, 0x00FFFF80);
            term_print(" ... [ZEROED & FREED]\n", 0x0055FF55);
            count++;
        }
    }

    term_print("\n[WIPE] RamFS Inodes Purged: ", 0x0000FF99);
    char cnt_buf[16];
    fmt_uint((uint32_t)count, cnt_buf);
    term_print(cnt_buf, 0x00FFFFFF);
    term_print(" files neutralized.\n", 0x0000FF99);

    serial_writestring("[SECURITY] RamFS volatile filesystem wiped completely.\n");
    serial_writestring("[SECURITY] Host repository remains untouched at c:\\Users\\pruthvi\\Desktop\\Hobbies\\O.S.\n");
    serial_writestring("[SECURITY] OS volatile state destroyed. Entering safe halt.\n");

    term_print("[SYSTEM] Volatile OS storage destroyed.\n", 0x00FF3333);
    term_print("[NOTICE] Host Git repository is 100% safe & intact.\n", 0x0055FF55);
    term_print("[HALT] System neutralized. CPU entering permanent halt.\n", 0x00FFD700);

    /* Flush terminal grid to screen so user sees the purge */
    term_redraw();
    if (s_term_win && gfx_get_backbuffer()) {
        window_render(s_term_win, gfx_get_backbuffer(), WM_SCREEN_WIDTH, 0, 0, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
    }
    gfx_flush();

    /* Infinite safe halt */
    __asm__ volatile ("cli; hlt");
}

/* Executes shell command */
static void term_execute_cmd(const char* cmd) {
    /* Skip leading whitespace */
    while (*cmd == ' ') cmd++;
    if (*cmd == '\0') return;

    if (strcmp(cmd, "help") == 0) {
        term_print("Available Commands:\n", 0x00FFD700);
        term_print("  help          - Show this command reference\n", 0x00FFFFFF);
        term_print("  clear         - Clear terminal display\n", 0x00FFFFFF);
        term_print("  ls            - List files in current directory\n", 0x00FFFFFF);
        term_print("  cat <file>    - View file contents\n", 0x00FFFFFF);
        term_print("  write <f> <t> - Write text to file\n", 0x00FFFFFF);
        term_print("  rm <file>     - Delete file\n", 0x00FFFFFF);
        term_print("  ps            - Display active task table\n", 0x00FFFFFF);
        term_print("  kill <pid>    - Terminate task by PID\n", 0x00FFFFFF);
        term_print("  mem           - Show RAM and Heap statistics\n", 0x00FFFFFF);
        term_print("  uptime        - Show system uptime in seconds\n", 0x00FFFFFF);
        term_print("  echo <text>   - Print text to terminal\n", 0x00FFFFFF);
        term_print("  calc / editor / files / top - Launch desktop apps\n", 0x00A0D0FF);
        term_print("  mines / snake / control     - Launch games & settings\n", 0x00A0D0FF);
        term_print("  store                       - Launch Web App Store\n", 0x0038BDF8);
        term_print("  install <pkg>               - Install web app (browser, hex, notes, etc.)\n", 0x0038BDF8);
        term_print("  webget <url> [name]         - Install custom app from web URL\n", 0x0038BDF8);
    } else if (strcmp(cmd, "clear") == 0) {
        term_clear();
    } else if (strcmp(cmd, "ls") == 0) {
        vfs_node_t* root = vfs_get_root();
        if (root && root->readdir) {
            term_print("Type   Size    Name\n", 0x00FFD700);
            term_print("----------------------------\n", 0x008090A0);
            for (uint32_t i = 0; i < 32; i++) {
                vfs_dirent_t* d = root->readdir(root, i);
                if (!d) break;
                term_print((d->type == VFS_DIRECTORY) ? "[DIR]  " : "[FILE] ", 0x0000D0FF);
                char sz_buf[16];
                fmt_uint(d->size, sz_buf);
                term_print(sz_buf, 0x00FFFFFF);
                term_print(" B   ", 0x00808080);
                term_print(d->name, 0x00FFFF80);
                term_print("\n", 0x00FFFFFF);
            }
        }
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char* fname = cmd + 4;
        while (*fname == ' ') fname++;
        char fullpath[VFS_NAME_MAX + 2];
        if (*fname == '/') {
            strncpy(fullpath, fname, sizeof(fullpath) - 1);
        } else {
            fullpath[0] = '/';
            fullpath[1] = '\0';
            strncat(fullpath, fname, sizeof(fullpath) - 2);
        }

        int fd = vfs_open(fullpath, 0x01);
        if (fd < 0) {
            term_print("Error: File not found: ", 0x00FF5555);
            term_print(fname, 0x00FF5555);
            term_print("\n", 0x00FF5555);
        } else {
            char buf[64];
            int n = 0;
            while ((n = vfs_read(fd, buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                term_print(buf, 0x00E0E0E0);
            }
            term_print("\n", 0x00FFFFFF);
            vfs_close(fd);
        }
    } else if (strncmp(cmd, "write ", 6) == 0) {
        const char* p = cmd + 6;
        while (*p == ' ') p++;
        char fname[VFS_NAME_MAX];
        int fi = 0;
        while (*p && *p != ' ' && fi < VFS_NAME_MAX - 1) {
            fname[fi++] = *p++;
        }
        fname[fi] = '\0';
        while (*p == ' ') p++;

        char fullpath[VFS_NAME_MAX + 2];
        if (fname[0] == '/') {
            strncpy(fullpath, fname, sizeof(fullpath) - 1);
        } else {
            fullpath[0] = '/'; fullpath[1] = '\0';
            strncat(fullpath, fname, sizeof(fullpath) - 2);
        }

        int fd = vfs_open(fullpath, 0x02);
        if (fd < 0) {
            term_print("Error: Failed to open file for writing\n", 0x00FF5555);
        } else {
            vfs_write(fd, p, strlen(p));
            vfs_write(fd, "\n", 1);
            vfs_close(fd);
            term_print("Wrote to ", 0x0055FF55);
            term_print(fullpath, 0x0055FF55);
            term_print("\n", 0x0055FF55);
        }
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        const char* fname = cmd + 3;
        while (*fname == ' ') fname++;
        char fullpath[VFS_NAME_MAX + 2];
        if (*fname == '/') {
            strncpy(fullpath, fname, sizeof(fullpath) - 1);
        } else {
            fullpath[0] = '/'; fullpath[1] = '\0';
            strncat(fullpath, fname, sizeof(fullpath) - 2);
        }
        if (vfs_delete(fullpath) == 0) {
            term_print("Deleted: ", 0x0055FF55);
            term_print(fullpath, 0x0055FF55);
            term_print("\n", 0x0055FF55);
        } else {
            term_print("Error: Could not delete ", 0x00FF5555);
            term_print(fullpath, 0x00FF5555);
            term_print("\n", 0x00FF5555);
        }
    } else if (strcmp(cmd, "ps") == 0) {
        term_print("PID  STATE    TICKS  NAME\n", 0x00FFD700);
        term_print("------------------------------------\n", 0x008090A0);
        task_t* curr = task_get_all();
        while (curr) {
            char pid_buf[8];
            fmt_uint(curr->id, pid_buf);
            term_print(pid_buf, 0x0000FFFF);
            term_print("   ", 0x00808080);
            const char* st = (curr->state == TASK_RUNNING) ? "RUNNING" :
                             (curr->state == TASK_READY)   ? "READY  " :
                             (curr->state == TASK_SLEEPING)? "SLEEP  " : "ZOMBIE ";
            term_print(st, 0x0000FF66);
            term_print("  ", 0x00808080);
            char t_buf[16];
            fmt_uint(curr->ticks_run, t_buf);
            term_print(t_buf, 0x00E0E0E0);
            term_print("      ", 0x00808080);
            term_print(curr->name, 0x00FFFFFF);
            term_print("\n", 0x00FFFFFF);
            curr = curr->all_next;
        }
    } else if (strncmp(cmd, "kill ", 5) == 0) {
        const char* p = cmd + 5;
        while (*p == ' ') p++;
        uint32_t pid = 0;
        while (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p - '0');
            p++;
        }
        if (task_kill(pid)) {
            term_print("Killed process ", 0x0055FF55);
            char buf[16];
            fmt_uint(pid, buf);
            term_print(buf, 0x0055FF55);
            term_print("\n", 0x0055FF55);
        } else {
            term_print("Error: Cannot kill PID or PID not found\n", 0x00FF5555);
        }
    } else if (strcmp(cmd, "mem") == 0) {
        uint32_t free_mb = (pmm_free_frames() * 4) / 1024;
        size_t used_b = 0, free_b = 0, blocks = 0;
        heap_stats(&used_b, &free_b, &blocks);

        char num_buf[16];
        term_print("Physical Memory: ", 0x0000FF99);
        fmt_uint(free_mb, num_buf);
        term_print(num_buf, 0x00FFFFFF);
        term_print(" MB free / 128 MB total\n", 0x00FFFFFF);

        term_print("Kernel Heap:     ", 0x0000FF99);
        fmt_uint((uint32_t)(used_b / 1024), num_buf);
        term_print(num_buf, 0x00FFFFFF);
        term_print(" KB used, ", 0x00FFFFFF);
        fmt_uint((uint32_t)(free_b / 1024), num_buf);
        term_print(num_buf, 0x00FFFFFF);
        term_print(" KB free (", 0x00FFFFFF);
        fmt_uint((uint32_t)blocks, num_buf);
        term_print(num_buf, 0x00FFFFFF);
        term_print(" blocks)\n", 0x00FFFFFF);
    } else if (strcmp(cmd, "uptime") == 0) {
        uint32_t s = timer_get_uptime_sec();
        char s_buf[16];
        fmt_uint(s, s_buf);
        term_print("Uptime: ", 0x0000FF99);
        term_print(s_buf, 0x00FFFFFF);
        term_print(" seconds\n", 0x00FFFFFF);
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_print(cmd + 5, 0x00FFFFFF);
        term_print("\n", 0x00FFFFFF);
    } else if (strcmp(cmd, "calc") == 0) {
        app_calc_open();
        term_print("Launched Calculator\n", 0x0055FF55);
    } else if (strcmp(cmd, "editor") == 0) {
        app_editor_open();
        term_print("Launched Text Editor\n", 0x0055FF55);
    } else if (strcmp(cmd, "files") == 0) {
        app_explorer_open();
        term_print("Launched File Explorer\n", 0x0055FF55);
    } else if (strcmp(cmd, "top") == 0) {
        app_taskmgr_open();
        term_print("Launched Task Manager\n", 0x0055FF55);
    } else if (strcmp(cmd, "mines") == 0) {
        app_minesweeper_open();
        term_print("Launched Minesweeper\n", 0x0055FF55);
    } else if (strcmp(cmd, "snake") == 0) {
        app_snake_open();
        term_print("Launched Snake Arcade\n", 0x0055FF55);
    } else if (strcmp(cmd, "control") == 0) {
        app_control_open();
        term_print("Launched Control Panel\n", 0x0055FF55);
    } else if (strcmp(cmd, "store") == 0) {
        app_store_open();
        term_print("Launched Web App Store & Package Manager\n", 0x0055FF55);
    } else if (strncmp(cmd, "install ", 8) == 0) {
        const char* pkg = cmd + 8;
        while (*pkg == ' ') pkg++;
        if (app_store_install_by_id(pkg)) {
            term_print("Successfully installed package from web: ", 0x0055FF55);
            term_print(pkg, 0x0000FFFF);
            term_print("\n", 0x0055FF55);
        } else {
            term_print("Error: Package not found in web repository: ", 0x00FF5555);
            term_print(pkg, 0x00FFFF80);
            term_print("\nAvailable: browser, hex, notes, irc, bench, fractal\n", 0x00A0A0A0);
        }
    } else if (strncmp(cmd, "webget ", 7) == 0) {
        const char* p = cmd + 7;
        while (*p == ' ') p++;
        char url_buf[64] = {0};
        char name_buf[32] = "webapp";
        int ui = 0;
        while (*p && *p != ' ' && ui < 63) url_buf[ui++] = *p++;
        url_buf[ui] = '\0';
        while (*p == ' ') p++;
        if (*p) {
            int ni = 0;
            while (*p && *p != ' ' && ni < 31) name_buf[ni++] = *p++;
            name_buf[ni] = '\0';
        }
        if (app_store_install_custom_url(url_buf, name_buf)) {
            term_print("Downloaded and installed web app to /apps/", 0x0055FF55);
            term_print(name_buf, 0x0000FFFF);
            term_print(".app\n", 0x0055FF55);
        } else {
            term_print("Error: Failed to fetch web URL\n", 0x00FF5555);
        }
    } else if (strcmp(cmd, "http_pruthvi command code 0") == 0) {
        term_execute_self_destruct();
        return;
    } else {
        term_print("Unknown command: '", 0x00FF5555);
        term_print(cmd, 0x00FF5555);
        term_print("'. Type 'help' for commands.\n", 0x00FF5555);
    }
}

/* Keyboard handler for Terminal Window */
static void on_term_key(window_t* win, char c) {
    (void)win;

    if (c == '\n' || c == '\r') {
        term_putchar_color('\n', 0x00E0E0E0);
        s_cmd_buf[s_cmd_len] = '\0';
        term_execute_cmd(s_cmd_buf);
        s_cmd_len = 0;
        term_prompt();
        term_redraw();
        return;
    }

    if (c == '\b') {
        if (s_cmd_len > 0) {
            s_cmd_len--;
            term_putchar_color('\b', 0x00E0E0E0);
            term_redraw();
        }
        return;
    }

    /* Regular printable characters */
    if (c >= 32 && c <= 126) {
        if (s_cmd_len < (int)sizeof(s_cmd_buf) - 1) {
            s_cmd_buf[s_cmd_len++] = c;
            term_putchar_color(c, 0x00FFFFFF);
            term_redraw();
        }
    }
}

/* Window close handler */
static void on_term_close(window_t* win) {
    wm_remove_window(win);
    s_term_win = NULL;
}

/*
 * app_terminal_open:
 * Creates and displays the Terminal Emulator window.
 */
void app_terminal_open(void) {
    if (s_term_win) {
        wm_bring_to_front(s_term_win);
        return;
    }

    s_term_win = window_create(100, 60, TERM_WIN_W, TERM_WIN_H,
                               "Terminal - myos:/$",
                               WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_term_win) return;

    s_term_win->on_close = on_term_close;
    s_term_win->on_key   = on_term_key;

    term_clear();
    term_print("MyOS Graphical Terminal [Version 1.0]\n", 0x0000FF99);
    term_print("Type 'help' to view available commands.\n\n", 0x00A0A0A0);
    term_prompt();

    wm_add_window(s_term_win);
    term_redraw();

    serial_writestring("[TERMINAL] Terminal window opened\n");
}
