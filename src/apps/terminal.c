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
#include "drivers/pci.h"
#include "drivers/net.h"
#include "net/netdev.h"
#include "net/ethernet.h"
#include "net/ipv4.h"
#include "net/icmp.h"
#include "net/dns.h"
#include "net/http.h"
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

/* Helper: formats uint32_t into hexadecimal string with specified width */
static void fmt_hex(uint32_t val, int digits, char* buf) {
    for (int i = digits - 1; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        *buf++ = (nibble < 10) ? ('0' + nibble) : ('a' + (nibble - 10));
    }
    *buf = '\0';
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

    /* Invalidate client area so window manager recomposites and flushes to screen */
    wm_invalidate_rect(s_term_win->client_x, s_term_win->client_y,
                       s_term_win->client_width, s_term_win->client_height);
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
        term_print("  lspci         - List detected PCI devices\n", 0x00FFFFFF);
        term_print("  ifconfig      - Display network interface configuration\n", 0x00FFFFFF);
        term_print("  ping <host>   - Send ICMP echo requests\n", 0x00FFFFFF);
        term_print("  curl <url>    - Fetch HTTP web resource via TCP/IP\n", 0x00FFFFFF);
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
    } else if (strcmp(cmd, "lspci") == 0) {
        uint32_t count = pci_get_device_count();
        term_print("B:D.F   Vendor:Dev  Class / Type           IRQ  BAR0\n", 0x00FFD700);
        term_print("----------------------------------------------------------\n", 0x008090A0);
        for (uint32_t i = 0; i < count; i++) {
            const pci_device_t* d = pci_get_device(i);
            if (!d) continue;

            /* Format B:D.F */
            char hbus[4], hdev[4];
            fmt_hex(d->bus, 2, hbus);
            fmt_hex(d->slot, 2, hdev);
            char bdf[10];
            bdf[0] = hbus[0]; bdf[1] = hbus[1]; bdf[2] = ':';
            bdf[3] = hdev[0]; bdf[4] = hdev[1]; bdf[5] = '.';
            bdf[6] = (char)('0' + d->func); bdf[7] = ' '; bdf[8] = '\0';
            term_print(bdf, 0x0038BDF8);

            /* Format Vendor:Device */
            char hven[6], hdevid[6];
            fmt_hex(d->vendor_id, 4, hven);
            fmt_hex(d->device_id, 4, hdevid);
            char vd[14];
            int vi = 0;
            for (int k = 0; k < 4; k++) vd[vi++] = hven[k];
            vd[vi++] = ':';
            for (int k = 0; k < 4; k++) vd[vi++] = hdevid[k];
            vd[vi++] = ' '; vd[vi++] = ' '; vd[vi] = '\0';
            term_print(vd, 0x00FFD700);

            /* Format Class string padded to 23 chars */
            const char* cls = pci_class_to_string(d->class_code, d->subclass);
            char cls_pad[26];
            int ci = 0;
            while (cls[ci] && ci < 22) {
                cls_pad[ci] = cls[ci];
                ci++;
            }
            while (ci < 23) {
                cls_pad[ci++] = ' ';
            }
            cls_pad[ci] = '\0';
            term_print(cls_pad, 0x00E0E0E0);

            /* Format IRQ */
            char irq_buf[8];
            fmt_uint(d->irq_line, irq_buf);
            char irq_pad[6];
            irq_pad[0] = (d->irq_line < 10) ? ' ' : irq_buf[0];
            irq_pad[1] = (d->irq_line < 10) ? irq_buf[0] : irq_buf[1];
            irq_pad[2] = ' '; irq_pad[3] = ' '; irq_pad[4] = '\0';
            term_print(irq_pad, 0x0055FF55);

            /* Format BAR0 */
            if (d->bar_type[0] == PCI_BAR_TYPE_NONE) {
                term_print("none\n", 0x00808080);
            } else {
                char hbar[10];
                fmt_hex(d->bar[0], 8, hbar);
                char bar_str[14];
                bar_str[0] = '0'; bar_str[1] = 'x';
                for (int k = 0; k < 8; k++) bar_str[2 + k] = hbar[k];
                bar_str[10] = '\n'; bar_str[11] = '\0';
                term_print(bar_str, (d->bar_type[0] == PCI_BAR_TYPE_IO) ? 0x00FFAA55 : 0x00A0D0FF);
            }
        }
    } else if (strcmp(cmd, "ifconfig") == 0 || strcmp(cmd, "netstat") == 0) {
        netdev_t* dev = netdev_get_default();
        if (!dev) {
            term_print("No network interface configured.\n", 0x00FF5555);
        } else {
            term_print(dev->name, 0x0038BDF8);
            term_print(": flags=UP,BROADCAST,RUNNING  mtu 1500\n", 0x0055FF55);

            char ip_str[24], mask_str[24], gw_str[24], dns_str[24];
            ip_to_str(dev->ip, ip_str);
            ip_to_str(dev->netmask, mask_str);
            ip_to_str(dev->gateway, gw_str);
            ip_to_str(dev->dns, dns_str);

            term_print("        inet ", 0x00808080);
            term_print(ip_str, 0x00FFFFFF);
            term_print("  netmask ", 0x00808080);
            term_print(mask_str, 0x00FFFFFF);
            term_print("\n        gateway ", 0x00808080);
            term_print(gw_str, 0x0034D399);
            term_print("  dns ", 0x00808080);
            term_print(dns_str, 0x0034D399);

            char mac_str[24];
            mac_to_str(dev->mac, mac_str);
            term_print("\n        ether ", 0x00808080);
            term_print(mac_str, 0x00FBBF24);

            char s_rxp[16], s_rxb[16], s_txp[16], s_txb[16];
            fmt_uint(dev->rx_packets, s_rxp);
            fmt_uint(dev->rx_bytes, s_rxb);
            fmt_uint(dev->tx_packets, s_txp);
            fmt_uint(dev->tx_bytes, s_txb);

            term_print("\n        RX packets ", 0x00808080);
            term_print(s_rxp, 0x00FFFFFF);
            term_print("  bytes ", 0x00808080);
            term_print(s_rxb, 0x00FFFFFF);

            term_print("\n        TX packets ", 0x00808080);
            term_print(s_txp, 0x00FFFFFF);
            term_print("  bytes ", 0x00808080);
            term_print(s_txb, 0x00FFFFFF);
            term_print("\n", 0x00808080);
        }
    } else if (strncmp(cmd, "ping", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* p = cmd + 4;
        while (*p == ' ') p++;
        if (*p == '\0') {
            term_print("Usage: ping <ip_or_hostname>\n", 0x00FF8080);
            return;
        }

        char target_host[64];
        int ti = 0;
        while (*p && *p != ' ' && ti < 63) {
            target_host[ti++] = *p++;
        }
        target_host[ti] = '\0';

        uint32_t target_ip = 0;
        if (!dns_resolve(target_host, &target_ip)) {
            term_print("ping: cannot resolve '", 0x00FF5555);
            term_print(target_host, 0x00FF5555);
            term_print("': Unknown host\n", 0x00FF5555);
            return;
        }

        char ip_str[24];
        ip_to_str(target_ip, ip_str);

        term_print("PING ", 0x0038BDF8);
        term_print(target_host, 0x00FFFFFF);
        term_print(" (", 0x00808080);
        term_print(ip_str, 0x0034D399);
        term_print("): 32 data bytes\n", 0x00808080);
        term_redraw();

        uint32_t received = 0;
        for (uint16_t seq = 1; seq <= 4; seq++) {
            uint32_t rtt = 0;
            bool ok = icmp_ping(target_ip, seq, &rtt);
            if (ok) {
                char s_seq[8], s_rtt[16];
                fmt_uint(seq, s_seq);
                fmt_uint(rtt, s_rtt);
                term_print("32 bytes from ", 0x00CBD5E1);
                term_print(ip_str, 0x0034D399);
                term_print(": icmp_seq=", 0x00808080);
                term_print(s_seq, 0x00FFFFFF);
                term_print(" ttl=64 time=", 0x00808080);
                term_print(s_rtt, 0x00FBBF24);
                term_print(" ms\n", 0x00FBBF24);
                received++;
            } else {
                char s_seq[8];
                fmt_uint(seq, s_seq);
                term_print("Request timeout for icmp_seq ", 0x00F87171);
                term_print(s_seq, 0x00F87171);
                term_print("\n", 0x00F87171);
            }
            term_redraw();
            if (s_term_win && gfx_get_backbuffer()) {
                window_render(s_term_win, gfx_get_backbuffer(), WM_SCREEN_WIDTH, 0, 0, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
                gfx_flush();
            }
        }

        term_print("--- ", 0x00808080);
        term_print(target_host, 0x0038BDF8);
        term_print(" ping statistics ---\n", 0x00808080);
        char s_rec[8], s_loss[8];
        fmt_uint(received, s_rec);
        fmt_uint((4 - received) * 25, s_loss);
        term_print("4 packets transmitted, ", 0x00CBD5E1);
        term_print(s_rec, 0x0034D399);
        term_print(" packets received, ", 0x00CBD5E1);
        term_print(s_loss, (received == 4) ? 0x0034D399 : 0x00F87171);
        term_print("% packet loss\n", 0x00CBD5E1);
    } else if (strncmp(cmd, "curl", 4) == 0 && (cmd[4] == ' ' || cmd[4] == '\0')) {
        const char* p = cmd + 4;
        while (*p == ' ') p++;
        if (*p == '\0') {
            term_print("Usage: curl <http://host/path>\n", 0x00FF8080);
            return;
        }

        char url_buf[128];
        int ui = 0;
        while (*p && *p != ' ' && ui < 127) {
            url_buf[ui++] = *p++;
        }
        url_buf[ui] = '\0';

        term_print("Connecting to ", 0x00A0A0A0);
        term_print(url_buf, 0x0038BDF8);
        term_print(" ...\n", 0x00A0A0A0);
        term_redraw();

        static char s_curl_resp[4096];
        int n = http_get(url_buf, s_curl_resp, sizeof(s_curl_resp));
        if (n >= 0) {
            term_print("----------------------------------------------------------------\n", 0x000284C7);
            /* Print HTTP response headers and body directly */
            term_print(s_curl_resp, 0x00E2E8F0);
            term_print("\n----------------------------------------------------------------\n", 0x000284C7);
            char s_len[16];
            fmt_uint((uint32_t)n, s_len);
            term_print("Transferred ", 0x0034D399);
            term_print(s_len, 0x00FFFFFF);
            term_print(" bytes via TCP port 80 [200 OK]\n", 0x0034D399);
        } else {
            char s_err[8];
            fmt_uint((uint32_t)(-n), s_err);
            term_print("curl: (7) Connection failed (error code ", 0x00F87171);
            term_print(s_err, 0x00F87171);
            term_print(")\n", 0x00F87171);
        }
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

/* Window resize handler */
static void on_term_resize(window_t* win, int new_w, int new_h) {
    (void)win;
    (void)new_w;
    (void)new_h;
    term_redraw();
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
                               WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_term_win) return;

    s_term_win->on_close  = on_term_close;
    s_term_win->on_key    = on_term_key;
    s_term_win->on_resize = on_term_resize;

    term_clear();
    term_print("MyOS Graphical Terminal [Version 1.0]\n", 0x0000FF99);
    term_print("Type 'help' to view available commands.\n\n", 0x00A0A0A0);
    term_prompt();

    wm_add_window(s_term_win);
    term_redraw();

    serial_writestring("[TERMINAL] Terminal window opened\n");
}
