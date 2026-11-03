#include "gui/startmenu.h"
#include "gui/theme.h"
#include "gui/lockscreen.h"
#include "gui/notification.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "lib/io.h"
#include "lib/string.h"
#include "apps/terminal.h"
#include "apps/explorer.h"
#include "apps/editor.h"
#include "apps/calc.h"
#include "apps/sheet.h"
#include "apps/taskmgr.h"
#include "apps/control.h"
#include "apps/minesweeper.h"
#include "apps/snake.h"

/* Forward declarations for new apps implemented in this wave */
extern void app_photos_open(void) __attribute__((weak));
extern void app_player_open(void) __attribute__((weak));
extern void app_breakout_open(void) __attribute__((weak));

#define MENU_X 8
#define MENU_W 270
#define MENU_H 390

static inline int startmenu_get_y(void) {
    if (theme_get_current_id() == THEME_WINDOWS_11) {
        return WM_TASKBAR_Y - MENU_H - 6;
    }
    return 30;
}

static bool s_is_open = false;

typedef struct {
    const char* name;
    const char* tag;
    uint32_t icon_color;
    void (*launch_fn)(void);
} start_item_t;

static const start_item_t s_items_col1[] = {
    { "Terminal",  ">_",  0x00A6E3A1, app_terminal_open },
    { "Files",     "Dir", 0x00F9E2AF, app_explorer_open },
    { "Editor",    "Doc", 0x0089B4FA, app_editor_open },
    { "Calc",      "+=",  0x00FAB387, app_calc_open },
    { "Sheets",    "#",   0x00A6E3A1, app_sheet_open },
    { "Task Mgr",  "^",   0x00F38BA8, app_taskmgr_open }
};

static const start_item_t s_items_col2[] = {
    { "Settings",  "*",   0x00CBA6F7, app_control_open },
    { "Photos",    "Img", 0x0089DCEB, app_photos_open },
    { "Music",     ">>",  0x00F5C2E7, app_player_open },
    { "Mines",     "M",   0x00F38BA8, app_minesweeper_open },
    { "Snake",     "~",   0x00A6E3A1, app_snake_open },
    { "Breakout",  "=",   0x00F9E2AF, app_breakout_open }
};

#define NUM_COL1 (int)(sizeof(s_items_col1) / sizeof(s_items_col1[0]))
#define NUM_COL2 (int)(sizeof(s_items_col2) / sizeof(s_items_col2[0]))

void startmenu_init(void) {
    s_is_open = false;
}

void startmenu_toggle(void) {
    s_is_open = !s_is_open;
}

void startmenu_open(void) {
    s_is_open = true;
}

void startmenu_close(void) {
    s_is_open = false;
}

bool startmenu_is_open(void) {
    return s_is_open;
}

static void system_reboot(void) {
    outb(0x64, 0xFE);
    __asm__ volatile ("cli");
    struct { uint16_t limit; uint32_t base; } __attribute__((packed)) null_idt = {0, 0};
    __asm__ volatile ("lidt %0; int3" : : "m"(null_idt));
    while (1) { __asm__ volatile ("hlt"); }
}

static void system_halt(void) {
    notification_push(NOTIFY_WARNING, "System Halting", "CPU entering safe halt...");
    __asm__ volatile ("cli");
    while (1) { __asm__ volatile ("hlt"); }
}

bool startmenu_handle_click(int32_t mx, int32_t my) {
    if (!s_is_open) {
        return false;
    }

    int menu_y = startmenu_get_y();

    /* Clicking outside start menu closes it */
    if (mx < MENU_X || mx >= MENU_X + MENU_W ||
        my < menu_y || my >= menu_y + MENU_H) {
        s_is_open = false;
        return true;
    }

    /* Check Column 1 clicks */
    int item_start_y = menu_y + 105;
    int item_h = 36;
    for (int i = 0; i < NUM_COL1; i++) {
        int iy = item_start_y + i * item_h;
        if (mx >= MENU_X + 10 && mx < MENU_X + 130 &&
            my >= iy && my < iy + 32) {
            s_is_open = false;
            if (s_items_col1[i].launch_fn) {
                s_items_col1[i].launch_fn();
            }
            return true;
        }
    }

    /* Check Column 2 clicks */
    for (int i = 0; i < NUM_COL2; i++) {
        int iy = item_start_y + i * item_h;
        if (mx >= MENU_X + 135 && mx < MENU_X + 258 &&
            my >= iy && my < iy + 32) {
            s_is_open = false;
            if (s_items_col2[i].launch_fn) {
                s_items_col2[i].launch_fn();
            }
            return true;
        }
    }

    /* Check Footer Action Bar: y = menu_y + MENU_H - 42 */
    int footer_y = menu_y + MENU_H - 42;
    if (my >= footer_y && my < footer_y + 30) {
        /* [ Lock ] at x + 10 .. x + 85 */
        if (mx >= MENU_X + 10 && mx < MENU_X + 85) {
            s_is_open = false;
            lockscreen_activate();
            return true;
        }
        /* [ Restart ] at x + 92 .. x + 175 */
        if (mx >= MENU_X + 92 && mx < MENU_X + 175) {
            s_is_open = false;
            system_reboot();
            return true;
        }
        /* [ Halt ] at x + 182 .. x + 258 */
        if (mx >= MENU_X + 182 && mx < MENU_X + 258) {
            s_is_open = false;
            system_halt();
            return true;
        }
    }

    return true; /* Consumed click inside menu */
}

void startmenu_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme) {
    (void)backbuf; (void)fb_width; (void)fb_height;
    if (!s_is_open) {
        return;
    }

    int menu_y = startmenu_get_y();

    uint32_t bg_col     = theme ? theme->bg_panel        : 0x00181825;
    uint32_t border_col = theme ? theme->border_active    : 0x00CBA6F7;
    uint32_t text_col   = theme ? theme->text_primary    : 0x00CDD6F4;
    uint32_t sub_col    = theme ? theme->text_secondary  : 0x00A6ADC8;
    uint32_t card_col   = theme ? theme->bg_card         : 0x001E1E2E;
    uint32_t sub_border = theme ? theme->border_inactive : 0x0045475A;

    /* Main Menu Frame */
    gfx_fill_rect(MENU_X, menu_y, MENU_W, MENU_H, bg_col);
    gfx_draw_rect(MENU_X, menu_y, MENU_W, MENU_H, border_col);

    /* User Profile Header (height 54) */
    gfx_fill_rect(MENU_X + 1, menu_y + 1, MENU_W - 2, 54, card_col);

    /* Circular Monogram Badge [PV] */
    int av_cx = MENU_X + 28;
    int av_cy = menu_y + 28;
    gfx_fill_rect(av_cx - 16, av_cy - 16, 32, 32, theme ? theme->accent_primary : 0x00CBA6F7);
    gfx_draw_rect(av_cx - 16, av_cy - 16, 32, 32, 0x00FFFFFF);
    gfx_draw_string(av_cx - 8, av_cy - 8, "PV", 0x0011111B, 0, true);

    /* User Name & System Level */
    gfx_draw_string(MENU_X + 56, menu_y + 14, "Pruthviraj", text_col, 0, true);
    gfx_draw_string(MENU_X + 56, menu_y + 32, "Administrator (Ring 0)", sub_col, 0, true);

    /* Search Bar Placeholder */
    int search_y = menu_y + 64;
    gfx_fill_rect(MENU_X + 10, search_y, MENU_W - 20, 26, card_col);
    gfx_draw_rect(MENU_X + 10, search_y, MENU_W - 20, 26, sub_border);
    gfx_draw_string(MENU_X + 18, search_y + 5, "Search apps & docs...", sub_col, 0, true);

    /* Column 1 App Items */
    int item_start_y = menu_y + 105;
    int item_h = 36;
    for (int i = 0; i < NUM_COL1; i++) {
        int iy = item_start_y + i * item_h;
        gfx_fill_rect(MENU_X + 10, iy, 120, 32, card_col);
        gfx_draw_rect(MENU_X + 10, iy, 120, 32, sub_border);

        /* Icon badge */
        gfx_fill_rect(MENU_X + 14, iy + 4, 24, 24, s_items_col1[i].icon_color);
        gfx_draw_string(MENU_X + 17, iy + 8, s_items_col1[i].tag, 0x0011111B, 0, true);

        /* Label */
        gfx_draw_string(MENU_X + 44, iy + 8, s_items_col1[i].name, text_col, 0, true);
    }

    /* Column 2 App Items */
    for (int i = 0; i < NUM_COL2; i++) {
        int iy = item_start_y + i * item_h;
        gfx_fill_rect(MENU_X + 135, iy, 123, 32, card_col);
        gfx_draw_rect(MENU_X + 135, iy, 123, 32, sub_border);

        /* Icon badge */
        gfx_fill_rect(MENU_X + 139, iy + 4, 24, 24, s_items_col2[i].icon_color);
        gfx_draw_string(MENU_X + 142, iy + 8, s_items_col2[i].tag, 0x0011111B, 0, true);

        /* Label */
        gfx_draw_string(MENU_X + 168, iy + 8, s_items_col2[i].name, text_col, 0, true);
    }

    /* Footer Action Bar */
    int footer_y = menu_y + MENU_H - 46;
    gfx_fill_rect(MENU_X + 1, footer_y, MENU_W - 2, 45, card_col);
    gfx_draw_line(MENU_X + 1, footer_y, MENU_X + MENU_W - 2, footer_y, sub_border);

    /* [ Lock ] button */
    gfx_fill_rect(MENU_X + 10, footer_y + 8, 76, 28, bg_col);
    gfx_draw_rect(MENU_X + 10, footer_y + 8, 76, 28, theme ? theme->accent_primary : 0x00CBA6F7);
    gfx_draw_string(MENU_X + 24, footer_y + 14, "Lock", text_col, 0, true);

    /* [ Restart ] button */
    gfx_fill_rect(MENU_X + 92, footer_y + 8, 82, 28, theme ? theme->bg_card : 0x00313244);
    gfx_draw_rect(MENU_X + 92, footer_y + 8, 82, 28, 0x00F9E2AF);
    gfx_draw_string(MENU_X + 102, footer_y + 14, "Reboot", text_col, 0, true);

    /* [ Halt ] button */
    gfx_fill_rect(MENU_X + 180, footer_y + 8, 78, 28, theme ? theme->bg_card : 0x00313244);
    gfx_draw_rect(MENU_X + 180, footer_y + 8, 78, 28, 0x00F38BA8);
    gfx_draw_string(MENU_X + 196, footer_y + 14, "Halt", 0x00F38BA8, 0, true);
}
