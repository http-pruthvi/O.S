#include "apps/launcher.h"
#include "apps/calc.h"
#include "apps/editor.h"
#include "apps/sheet.h"
#include "apps/clock.h"
#include "apps/taskmgr.h"
#include "apps/explorer.h"
#include "apps/terminal.h"
#include "apps/minesweeper.h"
#include "apps/snake.h"
#include "apps/control.h"
#include "gui/wm.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/serial.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/* Forward declarations for future app launchers */
extern void app_calc_open(void);
extern void app_editor_open(void);
extern void app_sheet_open(void);
extern void app_clock_open(void);
extern void app_taskmgr_open(void);
extern void app_explorer_open(void);
extern void app_terminal_open(void);
extern void paint_demo_run(void);

/* Launcher State */
static bool s_start_menu_open = false;
static bool s_requested_exit   = false;

/* Selected / clicked desktop icon index (-1 for none) */
static int s_selected_icon = -1;

/* Structure defining a desktop shortcut icon */
typedef struct {
    int x;
    int y;
    int w;
    int h;
    const char* label;
    void (*launch_fn)(void);
} desktop_icon_t;

/* Start Menu Item definition */
typedef struct {
    const char* title;
    void (*action_fn)(void);
    bool is_separator_after;
} menu_item_t;

/* System Monitor Window reference */
static window_t* s_win_sysmon = NULL;
static window_t* s_win_about  = NULL;

/* Callback for Sysmon Window Close */
static void on_sysmon_close(window_t* win) {
    (void)win;
    if (s_win_sysmon) {
        wm_remove_window(s_win_sysmon);
        s_win_sysmon = NULL;
    }
}

/* Callback for About Window Close */
static void on_about_close(window_t* win) {
    (void)win;
    if (s_win_about) {
        wm_remove_window(s_win_about);
        s_win_about = NULL;
    }
}

/* Helper to convert integer to string */
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

/* Callback: Refresh System Monitor Stats */
static void on_btn_refresh_sysmon(widget_t* w, void* data) {
    (void)w;
    widget_t** labels = (widget_t**)data;
    if (!labels) return;

    /* PMM free frames */
    uint32_t free_f = pmm_free_frames();
    char pmm_buf[48] = "PMM Free Frames: ";
    int_to_str((int)free_f, pmm_buf + 17);
    widget_set_text(labels[0], pmm_buf);

    /* Heap used */
    size_t used_b = 0, free_b = 0, blocks = 0;
    heap_stats(&used_b, &free_b, &blocks);
    char heap_buf[48] = "Heap Used: ";
    int_to_str((int)(used_b / 1024), heap_buf + 11);
    int len = 0; while (heap_buf[len]) len++;
    heap_buf[len++] = ' '; heap_buf[len++] = 'K'; heap_buf[len++] = 'B';
    heap_buf[len] = '\0';
    widget_set_text(labels[1], heap_buf);
}

/* Launcher: Open System Monitor Window */
void launcher_launch_sysmon(void) {
    if (s_win_sysmon) {
        wm_bring_to_front(s_win_sysmon);
        return;
    }

    s_win_sysmon = window_create(540, 100, 420, 320, "System Monitor",
                                 WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_sysmon) return;

    widget_t* lbl_title = widget_create_label(20, 16, 380, 20, "Hardware & Memory Status", 0x001F487E);
    widget_add_to_window(s_win_sysmon, lbl_title);

    widget_t* lbl_ram = widget_create_label(20, 50, 380, 20, "Total RAM: 128 MB (Identity Paged)", 0x00101010);
    widget_add_to_window(s_win_sysmon, lbl_ram);

    uint32_t free_f = pmm_free_frames();
    char pmm_buf[48] = "PMM Free Frames: ";
    int_to_str((int)free_f, pmm_buf + 17);
    widget_t* lbl_pmm = widget_create_label(20, 80, 380, 20, pmm_buf, 0x00101010);
    widget_add_to_window(s_win_sysmon, lbl_pmm);

    widget_t* lbl_heap_base = widget_create_label(20, 110, 380, 20, "Heap Virtual Base: 0xD0000000", 0x00101010);
    widget_add_to_window(s_win_sysmon, lbl_heap_base);

    size_t used_b = 0, free_b = 0, blocks = 0;
    heap_stats(&used_b, &free_b, &blocks);
    char heap_buf[48] = "Heap Used: ";
    int_to_str((int)(used_b / 1024), heap_buf + 11);
    int len = 0; while (heap_buf[len]) len++;
    heap_buf[len++] = ' '; heap_buf[len++] = 'K'; heap_buf[len++] = 'B';
    heap_buf[len] = '\0';
    widget_t* lbl_heap = widget_create_label(20, 140, 380, 20, heap_buf, 0x00101010);
    widget_add_to_window(s_win_sysmon, lbl_heap);

    widget_t* lbl_res = widget_create_label(20, 170, 380, 20, "Display: 1024x768x32 Linear RGB", 0x00101010);
    widget_add_to_window(s_win_sysmon, lbl_res);

    /* Allocate persistent label pointers for refresh callback */
    static widget_t* s_mon_labels[2];
    s_mon_labels[0] = lbl_pmm;
    s_mon_labels[1] = lbl_heap;

    widget_t* btn_refresh = widget_create_button(20, 220, 180, 32, "Refresh Metrics", on_btn_refresh_sysmon, s_mon_labels);
    widget_add_to_window(s_win_sysmon, btn_refresh);

    s_win_sysmon->on_close = on_sysmon_close;
    wm_add_window(s_win_sysmon);
}

/* Callback for About Window Dismiss */
static void on_btn_about_dismiss(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_win_about) {
        wm_remove_window(s_win_about);
        s_win_about = NULL;
    }
}

/* Launcher: Open About Window */
void launcher_launch_about(void) {
    if (s_win_about) {
        wm_bring_to_front(s_win_about);
        return;
    }

    s_win_about = window_create(280, 180, 440, 270, "About MyOS",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_about) return;

    widget_t* lbl_app = widget_create_label(20, 16, 400, 20, "MyOS Desktop Environment v1.0", 0x001F487E);
    widget_add_to_window(s_win_about, lbl_app);

    widget_t* lbl_sub = widget_create_label(20, 42, 400, 20, "B.Tech Final Year Operating System Project", 0x00404040);
    widget_add_to_window(s_win_about, lbl_sub);

    widget_t* lbl_d1 = widget_create_label(20, 80, 400, 20, "- Multi-Window Z-Order Compositor", 0x00101010);
    widget_add_to_window(s_win_about, lbl_d1);

    widget_t* lbl_d2 = widget_create_label(20, 105, 400, 20, "- Reusable Widgets & Application Framework", 0x00101010);
    widget_add_to_window(s_win_about, lbl_d2);

    widget_t* lbl_d3 = widget_create_label(20, 130, 400, 20, "- Productivity Apps: Calc, Editor, Sheet, Clock", 0x00101010);
    widget_add_to_window(s_win_about, lbl_d3);

    widget_t* lbl_d4 = widget_create_label(20, 155, 400, 20, "- Taskbar, Start Menu & Desktop Icons", 0x00101010);
    widget_add_to_window(s_win_about, lbl_d4);

    widget_t* btn_ok = widget_create_button(160, 195, 120, 32, "Dismiss", on_btn_about_dismiss, NULL);
    widget_add_to_window(s_win_about, btn_ok);

    s_win_about->on_close = on_about_close;
    wm_add_window(s_win_about);
}

void launcher_launch_calc(void)   { app_calc_open(); }
void launcher_launch_editor(void) { app_editor_open(); }
void launcher_launch_sheet(void)  { app_sheet_open(); }
void launcher_launch_clock(void)  { app_clock_open(); }

static void on_action_exit(void) {
    serial_writestring("[LAUNCHER] Shutdown selected. Exiting desktop...\n");
    s_requested_exit = true;
}

static void on_action_paint(void) {
    serial_writestring("[LAUNCHER] Paint Demo selected.\n");
    paint_demo_run();
}

/* Desktop Icon Definitions */
static desktop_icon_t s_desktop_icons[] = {
    {  24,  24, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Calculator", launcher_launch_calc   },
    {  24, 110, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Editor",     launcher_launch_editor },
    {  24, 196, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Sheets",     launcher_launch_sheet  },
    {  24, 282, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Clock",      launcher_launch_clock  },
    {  24, 368, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "TaskMgr",    app_taskmgr_open       },
    {  24, 454, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "About",      launcher_launch_about  },
    { 114,  24, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Explorer",   app_explorer_open      },
    { 114, 110, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Terminal",   app_terminal_open      },
    { 114, 196, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "SysMon",     launcher_launch_sysmon },
    { 114, 282, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Mines",      app_minesweeper_open   },
    { 114, 368, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Snake",      app_snake_open         },
    { 114, 454, LAUNCHER_ICON_WIDTH, LAUNCHER_ICON_HEIGHT, "Control",    app_control_open       }
};
#define NUM_DESKTOP_ICONS (int)(sizeof(s_desktop_icons) / sizeof(s_desktop_icons[0]))

/* Start Menu Item Definitions */
static menu_item_t s_menu_items[] = {
    { "File Explorer",    app_explorer_open,      false },
    { "Task Manager",     app_taskmgr_open,       false },
    { "Terminal CLI",     app_terminal_open,      false },
    { "Text Editor",      launcher_launch_editor, false },
    { "Calculator",       launcher_launch_calc,   false },
    { "Spreadsheet",      launcher_launch_sheet,  false },
    { "Clock & Calendar", launcher_launch_clock,  false },
    { "Minesweeper",      app_minesweeper_open,   false },
    { "Snake Arcade",     app_snake_open,         false },
    { "Control Panel",    app_control_open,       false },
    { "Paint Demo",       on_action_paint,        true  }, /* Separator line after */
    { "Shutdown (Exit)",  on_action_exit,         false }
};
#define NUM_MENU_ITEMS (int)(sizeof(s_menu_items) / sizeof(s_menu_items[0]))

/*
 * launcher_init:
 * Resets launcher state upon desktop entry.
 */
void launcher_init(void) {
    s_start_menu_open = false;
    s_requested_exit   = false;
    s_selected_icon   = -1;
    s_win_sysmon      = NULL;
    s_win_about       = NULL;
}

bool launcher_is_start_menu_open(void) {
    return s_start_menu_open;
}

void launcher_toggle_start_menu(void) {
    s_start_menu_open = !s_start_menu_open;
    /* Invalidate menu area and Start button */
    wm_invalidate_rect(LAUNCHER_MENU_X, LAUNCHER_MENU_Y, LAUNCHER_MENU_WIDTH + 4, LAUNCHER_MENU_HEIGHT + 4);
    wm_invalidate_rect(0, WM_TASKBAR_Y, 86, WM_TASKBAR_HEIGHT);
}

void launcher_close_start_menu(void) {
    if (s_start_menu_open) {
        s_start_menu_open = false;
        wm_invalidate_rect(LAUNCHER_MENU_X, LAUNCHER_MENU_Y, LAUNCHER_MENU_WIDTH + 4, LAUNCHER_MENU_HEIGHT + 4);
        wm_invalidate_rect(0, WM_TASKBAR_Y, 86, WM_TASKBAR_HEIGHT);
    }
}

bool launcher_should_exit(void) {
    return s_requested_exit;
}

void launcher_reset_exit(void) {
    s_requested_exit = false;
}

/*
 * draw_icon_graphic:
 * Renders a stylized 32x32 mini icon glyph for a desktop shortcut.
 */
static void draw_icon_graphic(int icon_idx, int cx, int cy) {
    switch (icon_idx) {
        case 0: /* Calculator: dark gray body, cyan screen, 3x3 white key dots */
            gfx_fill_rect(cx - 16, cy - 18, 32, 36, 0x003A404D);
            gfx_draw_rect(cx - 16, cy - 18, 32, 36, 0x00FFFFFF);
            /* LCD screen */
            gfx_fill_rect(cx - 12, cy - 14, 24, 8, 0x008AE6B8);
            gfx_draw_rect(cx - 12, cy - 14, 24, 8, 0x00104020);
            /* Key grid */
            for (int r = 0; r < 3; r++) {
                for (int c = 0; c < 3; c++) {
                    gfx_fill_rect(cx - 11 + c * 8, cy - 2 + r * 6, 6, 4, 0x00D0D8E0);
                }
            }
            break;

        case 1: /* Text Editor: white paper with folded corner & blue text lines */
            gfx_fill_rect(cx - 14, cy - 18, 28, 36, 0x00FAFAFA);
            gfx_draw_rect(cx - 14, cy - 18, 28, 36, 0x00506070);
            /* Folded top-right corner */
            gfx_fill_rect(cx + 6, cy - 18, 8, 8, 0x00C0D0E0);
            /* Lines of text */
            for (int i = 0; i < 4; i++) {
                gfx_fill_rect(cx - 10, cy - 6 + i * 6, 20, 2, 0x002B4C7E);
            }
            break;

        case 2: /* Spreadsheet: green table header and grid */
            gfx_fill_rect(cx - 16, cy - 18, 32, 36, 0x00FFFFFF);
            gfx_draw_rect(cx - 16, cy - 18, 32, 36, 0x001F6E43);
            /* Green header */
            gfx_fill_rect(cx - 16, cy - 18, 32, 10, 0x001F6E43);
            /* Grid lines */
            gfx_draw_line(cx - 16, cy + 2,  cx + 15, cy + 2,  0x00A0C0A0);
            gfx_draw_line(cx - 16, cy + 10, cx + 15, cy + 10, 0x00A0C0A0);
            gfx_draw_line(cx - 5,  cy - 8,  cx - 5,  cy + 17, 0x00A0C0A0);
            gfx_draw_line(cx + 5,  cy - 8,  cx + 5,  cy + 17, 0x00A0C0A0);
            break;

        case 3: /* Clock: circular blue dial with white rim & clock hands */
            gfx_fill_rect(cx - 16, cy - 16, 32, 32, 0x001E3A5F);
            gfx_draw_rect(cx - 16, cy - 16, 32, 32, 0x00FFFFFF);
            /* Center dot */
            gfx_fill_rect(cx - 2, cy - 2, 4, 4, 0x00FFD700);
            /* Clock hands */
            gfx_draw_line(cx, cy, cx + 8, cy - 4, 0x00FFFFFF);
            gfx_draw_line(cx, cy, cx - 6, cy + 6, 0x00FF8040);
            break;

        case 4: /* Task Manager: Process activity chart */
            gfx_fill_rect(cx - 16, cy - 14, 32, 28, 0x001B365D);
            gfx_draw_rect(cx - 16, cy - 14, 32, 28, 0x00A0B0C0);
            /* Activity bars */
            gfx_fill_rect(cx - 12, cy + 2, 5, 8, 0x0000FF66);
            gfx_fill_rect(cx - 5, cy - 4, 5, 14, 0x0000FF66);
            gfx_fill_rect(cx + 2, cy - 8, 5, 18, 0x0000FF66);
            gfx_fill_rect(cx + 9, cy - 1, 5, 11, 0x0000FF66);
            break;

        case 5: /* About: blue circle badge with 'i' */
            gfx_fill_rect(cx - 15, cy - 15, 30, 30, 0x002060B0);
            gfx_draw_rect(cx - 15, cy - 15, 30, 30, 0x00FFFFFF);
            /* Letter 'i' */
            gfx_fill_rect(cx - 2, cy - 10, 4, 4, 0x00FFFFFF);
            gfx_fill_rect(cx - 2, cy - 3,  4, 11, 0x00FFFFFF);
            break;

        case 6: /* File Explorer: Manila Folder */
            gfx_fill_rect(cx - 16, cy - 14, 14, 6, 0x00D49B00); /* Tab */
            gfx_fill_rect(cx - 16, cy - 9, 32, 24, 0x00F5BA13);  /* Folder body */
            gfx_draw_rect(cx - 16, cy - 9, 32, 24, 0x00A06D00);
            /* Paper preview */
            gfx_fill_rect(cx - 10, cy - 12, 16, 5, 0x00FFFFFF);
            break;

        case 7: /* Terminal: Console box with prompt symbol */
            gfx_fill_rect(cx - 16, cy - 14, 32, 28, 0x000F141C);
            gfx_draw_rect(cx - 16, cy - 14, 32, 28, 0x0000FF99);
            gfx_draw_char(cx - 10, cy - 8, '>', 0x0000FF99, 0x000F141C, false);
            gfx_fill_rect(cx + 1, cy + 4, 6, 2, 0x0000FF99);
            break;

        case 8: /* System Monitor: monitor chassis with waveform */
            gfx_fill_rect(cx - 16, cy - 16, 32, 24, 0x002A303C);
            gfx_draw_rect(cx - 16, cy - 16, 32, 24, 0x00D0D8E0);
            /* Screen */
            gfx_fill_rect(cx - 13, cy - 13, 26, 18, 0x000F1722);
            /* Waveform */
            gfx_draw_line(cx - 11, cy - 4, cx - 5, cy - 4, 0x0000FF80);
            gfx_draw_line(cx - 5,  cy - 4, cx - 2, cy - 10, 0x0000FF80);
            gfx_draw_line(cx - 2,  cy - 10, cx + 2, cy + 2, 0x0000FF80);
            gfx_draw_line(cx + 2,  cy + 2, cx + 6, cy - 4, 0x0000FF80);
            gfx_draw_line(cx + 6,  cy - 4, cx + 11, cy - 4, 0x0000FF80);
            /* Stand */
            gfx_fill_rect(cx - 4, cy + 8, 8, 4, 0x00506070);
            gfx_fill_rect(cx - 8, cy + 12, 16, 2, 0x00506070);
            break;

        case 9: /* Minesweeper: gray tile with spike mine */
            gfx_fill_rect(cx - 15, cy - 15, 30, 30, 0x00C0C0C0);
            gfx_draw_rect(cx - 15, cy - 15, 30, 30, 0x00707070);
            /* Mine central body */
            gfx_fill_rect(cx - 6, cy - 6, 12, 12, 0x00101010);
            gfx_fill_rect(cx - 8, cy - 4, 16, 8, 0x00101010);
            gfx_fill_rect(cx - 4, cy - 8, 8, 16, 0x00101010);
            /* Spikes */
            gfx_draw_line(cx - 10, cy, cx + 10, cy, 0x00101010);
            gfx_draw_line(cx, cy - 10, cx, cy + 10, 0x00101010);
            gfx_draw_line(cx - 7, cy - 7, cx + 7, cy + 7, 0x00101010);
            gfx_draw_line(cx - 7, cy + 7, cx + 7, cy - 7, 0x00101010);
            /* White glint */
            gfx_fill_rect(cx - 3, cy - 3, 3, 3, 0x00FFFFFF);
            break;

        case 10: /* Snake: green serpent with red apple */
            gfx_fill_rect(cx - 15, cy - 15, 30, 30, 0x00182818);
            gfx_draw_rect(cx - 15, cy - 15, 30, 30, 0x00336633);
            /* Snake body segments */
            gfx_fill_rect(cx - 10, cy - 8, 8, 8, 0x0033CC33);
            gfx_fill_rect(cx - 2,  cy - 8, 8, 8, 0x0033CC33);
            gfx_fill_rect(cx - 2,  cy,     8, 8, 0x0022AA22);
            gfx_fill_rect(cx - 2,  cy + 6, 8, 8, 0x0022AA22);
            gfx_fill_rect(cx + 4,  cy + 6, 8, 8, 0x0022AA22);
            /* Red apple */
            gfx_fill_rect(cx + 4, cy - 6, 6, 6, 0x00EE2222);
            gfx_fill_rect(cx + 6, cy - 8, 2, 2, 0x0033AA33); /* Stem */
            break;

        case 11: /* Control Panel: slider switches and gear palette */
            gfx_fill_rect(cx - 15, cy - 15, 30, 30, 0x003A4D59);
            gfx_draw_rect(cx - 15, cy - 15, 30, 30, 0x009BB5C4);
            /* Slider tracks */
            gfx_fill_rect(cx - 10, cy - 7, 20, 3, 0x00202B33);
            gfx_fill_rect(cx - 10, cy + 3, 20, 3, 0x00202B33);
            /* Slider knobs */
            gfx_fill_rect(cx - 4, cy - 10, 6, 9, 0x0000C0FF);
            gfx_fill_rect(cx + 3, cy,      6, 9, 0x00FFB020);
            break;

        default:
            gfx_fill_rect(cx - 14, cy - 14, 28, 28, 0x00808080);
            break;
    }
}

/*
 * launcher_render_desktop_icons:
 * Draws all desktop shortcut icons on the desktop background.
 */
void launcher_render_desktop_icons(int clip_x, int clip_y, int clip_w, int clip_h) {
    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    for (int i = 0; i < NUM_DESKTOP_ICONS; i++) {
        desktop_icon_t* icon = &s_desktop_icons[i];

        if (icon->x + icon->w <= clip_x || icon->x >= clip_r ||
            icon->y + icon->h <= clip_y || icon->y >= clip_b) {
            continue; /* Completely outside dirty rectangle */
        }

        /* Selection highlight box */
        if (s_selected_icon == i) {
            gfx_fill_rect(icon->x, icon->y, icon->w, icon->h, 0x00253852);
            gfx_draw_rect(icon->x, icon->y, icon->w, icon->h, 0x005080B0);
        }

        /* Icon Graphic centered in top half of box */
        int center_x = icon->x + icon->w / 2;
        int center_y = icon->y + 22;
        draw_icon_graphic(i, center_x, center_y);

        /* Icon Label centered underneath */
        int text_len = (int)strlen(icon->label);
        int text_x = center_x - (text_len * FONT_GLYPH_WIDTH) / 2;
        int text_y = icon->y + 50;

        /* Dark shadow for readability */
        gfx_draw_string(text_x + 1, text_y + 1, icon->label, 0x00000000, 0, false);
        gfx_draw_string(text_x, text_y, icon->label, 0x00FFFFFF, 0, false);
    }
}

/*
 * launcher_render_start_menu:
 * Renders the classic Windows-style Start Menu popup above the taskbar.
 */
void launcher_render_start_menu(int clip_x, int clip_y, int clip_w, int clip_h) {
    if (!s_start_menu_open) return;

    int mx = LAUNCHER_MENU_X;
    int my = LAUNCHER_MENU_Y;
    int mw = LAUNCHER_MENU_WIDTH;
    int mh = LAUNCHER_MENU_HEIGHT;

    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    if (mx + mw <= clip_x || mx >= clip_r || my + mh <= clip_y || my >= clip_b) {
        return; /* Outside dirty region */
    }

    /* 1. Menu background (Classic light gray) */
    gfx_fill_rect(mx, my, mw, mh, 0x00D4D0C8);

    /* 2. 3D Beveled Border: light top/left, dark bottom/right */
    gfx_draw_line(mx,          my,          mx + mw - 1, my,          0x00FFFFFF);
    gfx_draw_line(mx,          my,          mx,          my + mh - 1, 0x00FFFFFF);
    gfx_draw_line(mx + mw - 1, my,          mx + mw - 1, my + mh - 1, 0x00404040);
    gfx_draw_line(mx,          my + mh - 1, mx + mw - 1, my + mh - 1, 0x00404040);

    /* 3. Left Brand Sidebar (Dark Blue Banner) */
    int banner_w = 26;
    gfx_fill_rect(mx + 2, my + 2, banner_w, mh - 4, 0x001B3A68);
    /* Vertical Brand Letters */
    gfx_draw_string(mx + 9, my + 16, "M", 0x00FFFFFF, 0x001B3A68, true);
    gfx_draw_string(mx + 9, my + 34, "y", 0x00FFFFFF, 0x001B3A68, true);
    gfx_draw_string(mx + 9, my + 52, "O", 0x00FFFFFF, 0x001B3A68, true);
    gfx_draw_string(mx + 9, my + 70, "S", 0x00FFFFFF, 0x001B3A68, true);

    /* 4. Menu Items */
    int item_x = mx + banner_w + 6;
    int item_w = mw - banner_w - 10;
    int item_h = 28;
    int item_y = my + 6;

    for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        menu_item_t* item = &s_menu_items[i];

        /* Draw menu item text */
        gfx_draw_string(item_x + 6, item_y + 6, item->title, 0x00000000, 0x00D4D0C8, false);

        /* Draw separator line if configured */
        if (item->is_separator_after) {
            int sep_y = item_y + item_h + 2;
            gfx_draw_line(item_x, sep_y, item_x + item_w - 4, sep_y, 0x00808080);
            gfx_draw_line(item_x, sep_y + 1, item_x + item_w - 4, sep_y + 1, 0x00FFFFFF);
            item_y += 6;
        }

        item_y += item_h;
    }
}

/*
 * launcher_handle_mouse:
 * Processes mouse clicks for Start Menu and Desktop Icons.
 * Returns true if the click was consumed by the launcher.
 */
bool launcher_handle_mouse(int mx, int my, bool left_pressed) {
    if (!left_pressed) return false;

    /* 1. Handle Start Menu clicks if open */
    if (s_start_menu_open) {
        int sm_x = LAUNCHER_MENU_X;
        int sm_y = LAUNCHER_MENU_Y;
        int sm_w = LAUNCHER_MENU_WIDTH;
        int sm_h = LAUNCHER_MENU_HEIGHT;

        /* Clicked inside Start Menu */
        if (mx >= sm_x && mx < sm_x + sm_w && my >= sm_y && my < sm_y + sm_h) {
            int banner_w = 26;
            int item_y = sm_y + 6;
            int item_h = 28;

            for (int i = 0; i < NUM_MENU_ITEMS; i++) {
                menu_item_t* item = &s_menu_items[i];
                if (my >= item_y && my < item_y + item_h && mx >= sm_x + banner_w) {
                    /* Close Start Menu first */
                    launcher_close_start_menu();

                    /* Invoke selected action */
                    if (item->action_fn) {
                        item->action_fn();
                    }
                    return true;
                }

                if (item->is_separator_after) {
                    item_y += 6;
                }
                item_y += item_h;
            }
            return true;
        }

        /* Clicked outside Start Menu: close it */
        launcher_close_start_menu();

        /* If click was on Start button, toggle already handled or will toggle */
        if (my >= WM_TASKBAR_Y && mx >= 6 && mx < 78) {
            return true; /* Consumed closing click */
        }
    }

    /* 2. Check clicks on Desktop Shortcut Icons (only if not on taskbar and no window hit) */
    if (my < WM_TASKBAR_Y) {
        for (int i = 0; i < NUM_DESKTOP_ICONS; i++) {
            desktop_icon_t* icon = &s_desktop_icons[i];
            if (mx >= icon->x && mx < icon->x + icon->w &&
                my >= icon->y && my < icon->y + icon->h) {
                
                s_selected_icon = i;
                wm_invalidate_rect(icon->x, icon->y, icon->w, icon->h);

                /* Launch app */
                if (icon->launch_fn) {
                    icon->launch_fn();
                }
                return true;
            }
        }
    }

    /* Clear icon selection if clicked elsewhere on desktop */
    if (s_selected_icon != -1) {
        int prev = s_selected_icon;
        s_selected_icon = -1;
        wm_invalidate_rect(s_desktop_icons[prev].x, s_desktop_icons[prev].y,
                           s_desktop_icons[prev].w, s_desktop_icons[prev].h);
    }

    return false;
}
