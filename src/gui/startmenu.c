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
#include "apps/store.h"

/* Forward declarations for apps */
extern void app_photos_open(void) __attribute__((weak));
extern void app_player_open(void) __attribute__((weak));
extern void app_breakout_open(void) __attribute__((weak));

#define MENU_X 8
#define MENU_W 284
#define MENU_H 400

static inline int startmenu_get_y(void) {
    if (theme_get_current_id() == THEME_WINDOWS_11) {
        return WM_TASKBAR_Y - MENU_H - 6;
    }
    return 30;
}

static bool s_is_open = false;
static bool s_search_active = false;
static char s_search_query[32] = {0};
static int  s_search_len = 0;

typedef struct {
    const char* name;
    const char* tag;
    uint32_t icon_color;
    void (*launch_fn)(void);
} start_item_t;

/* Core Built-in Applications */
static start_item_t s_items_col1[] = {
    { "Terminal",  ">_",  0x0000E5FF, app_terminal_open },
    { "Files",     "Dir", 0x00F5BA13, app_explorer_open },
    { "Editor",    "Doc", 0x0060A5FA, app_editor_open },
    { "Calc",      "+=",  0x00FAB387, app_calc_open },
    { "Sheets",    "#",   0x00A6E3A1, app_sheet_open },
    { "Task Mgr",  "^",   0x0034D399, app_taskmgr_open }
};

static start_item_t s_items_col2[] = {
    { "App Store", "Web", 0x0038BDF8, app_store_open },
    { "Settings",  "*",   0x00A78BFA, app_control_open },
    { "Photos",    "Img", 0x0089DCEB, app_photos_open },
    { "Music",     ">>",  0x00F472B6, app_player_open },
    { "Mines",     "M",   0x00F38BA8, app_minesweeper_open },
    { "Breakout",  "=",   0x00FBBF24, app_breakout_open }
};

#define NUM_COL1 (int)(sizeof(s_items_col1) / sizeof(s_items_col1[0]))
#define NUM_COL2 (int)(sizeof(s_items_col2) / sizeof(s_items_col2[0]))

/* Dynamically registered installed web apps */
#define MAX_EXTRA_ITEMS 16
static start_item_t s_extra_items[MAX_EXTRA_ITEMS];
static int s_extra_count = 0;

void startmenu_register_app(const char* name, const char* tag, uint32_t icon_color, void (*launch_fn)(void)) {
    if (!name || s_extra_count >= MAX_EXTRA_ITEMS) return;

    /* Prevent duplicate registration */
    for (int i = 0; i < s_extra_count; i++) {
        if (strcmp(s_extra_items[i].name, name) == 0) return;
    }

    s_extra_items[s_extra_count].name = name;
    s_extra_items[s_extra_count].tag = tag ? tag : "App";
    s_extra_items[s_extra_count].icon_color = icon_color;
    s_extra_items[s_extra_count].launch_fn = launch_fn;
    s_extra_count++;
}

void startmenu_init(void) {
    s_is_open = false;
    s_search_active = false;
    s_search_query[0] = '\0';
    s_search_len = 0;
    s_extra_count = 0;
}

void startmenu_toggle(void) {
    s_is_open = !s_is_open;
    if (s_is_open) {
        s_search_active = false;
        s_search_query[0] = '\0';
        s_search_len = 0;
    }
}

void startmenu_open(void) {
    s_is_open = true;
    s_search_active = false;
}

void startmenu_open_search(void) {
    s_is_open = true;
    s_search_active = true;
    s_search_query[0] = '\0';
    s_search_len = 0;
}

void startmenu_close(void) {
    s_is_open = false;
    s_search_active = false;
    s_search_query[0] = '\0';
    s_search_len = 0;
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

/* Helper: Case-insensitive substring match */
static bool match_query(const char* str, const char* q) {
    if (!q || !*q) return true;
    if (!str) return false;

    char s_low[48], q_low[32];
    int si = 0;
    while (str[si] && si < 47) {
        char c = str[si];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        s_low[si++] = c;
    }
    s_low[si] = '\0';

    int qi = 0;
    while (q[qi] && qi < 31) {
        char c = q[qi];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        q_low[qi++] = c;
    }
    q_low[qi] = '\0';

    return (strstr(s_low, q_low) != NULL);
}

/* Collect filtered search results */
static int collect_search_results(start_item_t* out_results, int max_out) {
    int count = 0;
    if (s_search_len == 0) return 0;

    /* Search col 1 */
    for (int i = 0; i < NUM_COL1 && count < max_out; i++) {
        if (match_query(s_items_col1[i].name, s_search_query)) {
            out_results[count++] = s_items_col1[i];
        }
    }
    /* Search col 2 */
    for (int i = 0; i < NUM_COL2 && count < max_out; i++) {
        if (match_query(s_items_col2[i].name, s_search_query)) {
            out_results[count++] = s_items_col2[i];
        }
    }
    /* Search extra installed web apps */
    for (int i = 0; i < s_extra_count && count < max_out; i++) {
        if (match_query(s_extra_items[i].name, s_search_query)) {
            out_results[count++] = s_extra_items[i];
        }
    }
    return count;
}

/* Keyboard handler when Start Menu / Search is open */
bool startmenu_handle_key(char c) {
    if (!s_is_open) return false;

    if (c == 27) { /* ESC: close start menu */
        startmenu_close();
        wm_redraw_all();
        return true;
    }

    if (c == '\b') { /* Backspace */
        if (s_search_len > 0) {
            s_search_len--;
            s_search_query[s_search_len] = '\0';
            wm_redraw_all();
        }
        return true;
    }

    if (c == '\n' || c == '\r') { /* Enter: launch top match */
        start_item_t matches[8];
        int n = collect_search_results(matches, 8);
        if (n > 0) {
            startmenu_close();
            if (matches[0].launch_fn) matches[0].launch_fn();
            wm_redraw_all();
            return true;
        }
        return true;
    }

    /* Printable characters (a-z, 0-9, space, symbols) */
    if ((unsigned char)c >= 32 && (unsigned char)c <= 126) {
        if (s_search_len < 28) {
            s_search_active = true;
            s_search_query[s_search_len++] = c;
            s_search_query[s_search_len] = '\0';
            wm_redraw_all();
        }
        return true;
    }

    return false;
}

bool startmenu_handle_click(int32_t mx, int32_t my) {
    if (!s_is_open) return false;

    int menu_y = startmenu_get_y();

    /* Clicking outside closes the start menu */
    if (mx < MENU_X || mx >= MENU_X + MENU_W ||
        my < menu_y || my >= menu_y + MENU_H) {
        startmenu_close();
        return true;
    }

    /* Check Search Bar click: y = menu_y + 64, h = 28 */
    int search_y = menu_y + 64;
    if (mx >= MENU_X + 10 && mx < MENU_X + MENU_W - 10 &&
        my >= search_y && my < search_y + 28) {
        s_search_active = true;
        /* If clicked near right edge, clear search */
        if (mx >= MENU_X + MENU_W - 35 && s_search_len > 0) {
            s_search_len = 0;
            s_search_query[0] = '\0';
        }
        return true;
    }

    /* If search query active, check filtered search results clicks */
    if (s_search_len > 0) {
        start_item_t matches[8];
        int n = collect_search_results(matches, 6);
        int item_start_y = menu_y + 105;
        for (int i = 0; i < n; i++) {
            int iy = item_start_y + i * 38;
            if (mx >= MENU_X + 10 && mx < MENU_X + MENU_W - 10 &&
                my >= iy && my < iy + 34) {
                startmenu_close();
                if (matches[i].launch_fn) matches[i].launch_fn();
                return true;
            }
        }
        return true;
    }

    /* Normal mode: Check Column 1 clicks */
    int item_start_y = menu_y + 105;
    int item_h = 36;
    for (int i = 0; i < NUM_COL1; i++) {
        int iy = item_start_y + i * item_h;
        if (mx >= MENU_X + 10 && mx < MENU_X + 138 &&
            my >= iy && my < iy + 32) {
            startmenu_close();
            if (s_items_col1[i].launch_fn) s_items_col1[i].launch_fn();
            return true;
        }
    }

    /* Normal mode: Check Column 2 clicks */
    for (int i = 0; i < NUM_COL2; i++) {
        int iy = item_start_y + i * item_h;
        if (mx >= MENU_X + 144 && mx < MENU_X + 274 &&
            my >= iy && my < iy + 32) {
            startmenu_close();
            if (s_items_col2[i].launch_fn) s_items_col2[i].launch_fn();
            return true;
        }
    }

    /* Check Footer Action Bar */
    int footer_y = menu_y + MENU_H - 44;
    if (my >= footer_y && my < footer_y + 32) {
        /* [ Lock ] */
        if (mx >= MENU_X + 10 && mx < MENU_X + 88) {
            startmenu_close();
            lockscreen_activate();
            return true;
        }
        /* [ Restart ] */
        if (mx >= MENU_X + 96 && mx < MENU_X + 182) {
            startmenu_close();
            system_reboot();
            return true;
        }
        /* [ Halt ] */
        if (mx >= MENU_X + 190 && mx < MENU_X + 274) {
            startmenu_close();
            system_halt();
            return true;
        }
    }

    return true;
}

void startmenu_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme) {
    (void)backbuf; (void)fb_width; (void)fb_height;
    if (!s_is_open) return;

    int menu_y = startmenu_get_y();

    uint32_t bg_col     = theme ? theme->bg_panel        : 0x00181825;
    uint32_t border_col = theme ? theme->border_active    : 0x000078D4;
    uint32_t text_col   = theme ? theme->text_primary    : 0x00FFFFFF;
    uint32_t sub_col    = theme ? theme->text_secondary  : 0x00A0A0A0;
    uint32_t card_col   = theme ? theme->bg_card         : 0x00222222;
    uint32_t sub_border = theme ? theme->border_inactive : 0x00333333;

    /* Main Menu Frame with 1px border */
    gfx_fill_rect(MENU_X, menu_y, MENU_W, MENU_H, bg_col);
    gfx_draw_rect(MENU_X, menu_y, MENU_W, MENU_H, border_col);

    /* User Profile Header */
    gfx_fill_rect(MENU_X + 1, menu_y + 1, MENU_W - 2, 54, card_col);

    /* Circular Monogram Badge [PV] */
    int av_cx = MENU_X + 28;
    int av_cy = menu_y + 28;
    gfx_fill_rect(av_cx - 16, av_cy - 16, 32, 32, 0x000078D4);
    gfx_draw_rect(av_cx - 16, av_cy - 16, 32, 32, 0x0060A5FA);
    gfx_draw_string(av_cx - 8, av_cy - 8, "PV", 0x00FFFFFF, 0, true);

    /* User Name & System Level */
    gfx_draw_string(MENU_X + 56, menu_y + 14, "Pruthviraj", text_col, 0, true);
    gfx_draw_string(MENU_X + 56, menu_y + 32, "Administrator (Ring 0)", sub_col, 0, true);

    /* Interactive Windows Search Bar */
    int search_y = menu_y + 64;
    uint32_t sbox_bd = s_search_active ? 0x000078D4 : sub_border;
    gfx_fill_rect(MENU_X + 10, search_y, MENU_W - 20, 28, 0x001F1F1F);
    gfx_draw_rect(MENU_X + 10, search_y, MENU_W - 20, 28, sbox_bd);

    /* Vector Search Icon */
    int sic_x = MENU_X + 18;
    int sic_y = search_y + 8;
    gfx_draw_rect(sic_x, sic_y, 7, 7, 0x000078D4);
    gfx_draw_line(sic_x + 6, sic_y + 6, sic_x + 9, sic_y + 9, 0x000078D4);

    if (s_search_len == 0) {
        gfx_draw_string(MENU_X + 32, search_y + 6, "Search apps & files...", 0x00707070, 0, true);
        if (s_search_active) {
            gfx_fill_rect(MENU_X + 32, search_y + 6, 2, 16, 0x000078D4);
        }
    } else {
        gfx_draw_string(MENU_X + 32, search_y + 6, s_search_query, 0x00FFFFFF, 0, true);
        int cur_x = MENU_X + 32 + s_search_len * FONT_GLYPH_WIDTH;
        if (cur_x < MENU_X + MENU_W - 35) {
            gfx_fill_rect(cur_x, search_y + 6, 2, 16, 0x000078D4);
        }
        /* Clear button [x] */
        gfx_draw_string(MENU_X + MENU_W - 26, search_y + 6, "x", 0x009E9E9E, 0, true);
    }

    /* Content Area: Filtered Results vs Pinned 2-Column Grid */
    if (s_search_len > 0) {
        start_item_t matches[8];
        int n = collect_search_results(matches, 6);
        int item_start_y = menu_y + 105;

        if (n == 0) {
            gfx_draw_string(MENU_X + 20, item_start_y + 30, "No matching applications found.", 0x0071717A, 0, true);
            gfx_draw_string(MENU_X + 20, item_start_y + 55, "Try 'terminal', 'store', 'calc'...", 0x0052525B, 0, true);
        } else {
            for (int i = 0; i < n; i++) {
                int iy = item_start_y + i * 38;
                bool is_first = (i == 0);
                uint32_t rcard_bg = is_first ? 0x002A2A2A : card_col;
                uint32_t rcard_bd = is_first ? 0x000078D4 : sub_border;

                gfx_fill_rect(MENU_X + 10, iy, MENU_W - 20, 34, rcard_bg);
                gfx_draw_rect(MENU_X + 10, iy, MENU_W - 20, 34, rcard_bd);

                /* Icon badge */
                gfx_fill_rect(MENU_X + 16, iy + 5, 24, 24, matches[i].icon_color);
                gfx_draw_string(MENU_X + 19, iy + 9, matches[i].tag, 0x0011111B, 0, true);

                /* Title */
                gfx_draw_string(MENU_X + 48, iy + 9, matches[i].name, text_col, 0, true);

                if (is_first) {
                    gfx_draw_string(MENU_X + MENU_W - 90, iy + 9, "[Enter]", 0x0060A5FA, 0, true);
                }
            }
        }
    } else {
        /* Standard Pinned Grid */
        int item_start_y = menu_y + 105;
        int item_h = 36;
        for (int i = 0; i < NUM_COL1; i++) {
            int iy = item_start_y + i * item_h;
            gfx_fill_rect(MENU_X + 10, iy, 126, 32, card_col);
            gfx_draw_rect(MENU_X + 10, iy, 126, 32, sub_border);

            gfx_fill_rect(MENU_X + 14, iy + 4, 24, 24, s_items_col1[i].icon_color);
            gfx_draw_string(MENU_X + 17, iy + 8, s_items_col1[i].tag, 0x0011111B, 0, true);
            gfx_draw_string(MENU_X + 44, iy + 8, s_items_col1[i].name, text_col, 0, true);
        }

        for (int i = 0; i < NUM_COL2; i++) {
            int iy = item_start_y + i * item_h;
            gfx_fill_rect(MENU_X + 144, iy, 128, 32, card_col);
            gfx_draw_rect(MENU_X + 144, iy, 128, 32, sub_border);

            gfx_fill_rect(MENU_X + 148, iy + 4, 24, 24, s_items_col2[i].icon_color);
            gfx_draw_string(MENU_X + 151, iy + 8, s_items_col2[i].tag, 0x0011111B, 0, true);
            gfx_draw_string(MENU_X + 176, iy + 8, s_items_col2[i].name, text_col, 0, true);
        }
    }

    /* Footer Action Bar */
    int footer_y = menu_y + MENU_H - 46;
    gfx_fill_rect(MENU_X + 1, footer_y, MENU_W - 2, 45, card_col);
    gfx_draw_line(MENU_X + 1, footer_y, MENU_X + MENU_W - 2, footer_y, sub_border);

    /* [ Lock ] button */
    gfx_fill_rect(MENU_X + 10, footer_y + 8, 76, 28, bg_col);
    gfx_draw_rect(MENU_X + 10, footer_y + 8, 76, 28, 0x000078D4);
    gfx_draw_string(MENU_X + 24, footer_y + 14, "Lock", text_col, 0, true);

    /* [ Reboot ] button */
    gfx_fill_rect(MENU_X + 94, footer_y + 8, 80, 28, bg_col);
    gfx_draw_rect(MENU_X + 94, footer_y + 8, 80, 28, 0x00F9E2AF);
    gfx_draw_string(MENU_X + 108, footer_y + 14, "Reboot", text_col, 0, true);

    /* [ Halt ] button */
    gfx_fill_rect(MENU_X + 184, footer_y + 8, 88, 28, bg_col);
    gfx_draw_rect(MENU_X + 184, footer_y + 8, 88, 28, 0x00F38BA8);
    gfx_draw_string(MENU_X + 204, footer_y + 14, "Halt", 0x00F38BA8, 0, true);
}
