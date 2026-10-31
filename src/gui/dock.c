#include "gui/dock.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "apps/terminal.h"
#include "apps/explorer.h"
#include "apps/editor.h"
#include "apps/calc.h"
#include "apps/sheet.h"
#include "apps/settings.h"
#include "apps/photos.h"
#include "apps/player.h"
#include "apps/breakout.h"
#include "apps/minesweeper.h"
#include "apps/snake.h"
#include "apps/taskmgr.h"
#include "gfx/gfx.h"
#include "gfx/font.h"

typedef struct {
    const char* label;
    void (*launch_fn)(void);
} dock_item_t;

static const dock_item_t s_dock_items[] = {
    { "Term",     app_terminal_open    },
    { "Files",    app_explorer_open    },
    { "Editor",   app_editor_open      },
    { "Calc",     app_calc_open        },
    { "Sheets",   app_sheet_open       },
    { "Settings", app_settings_open    },
    { "Photos",   app_photos_open      },
    { "Music",    app_player_open      },
    { "Breakout", app_breakout_open    },
    { "Mines",    app_minesweeper_open },
    { "Snake",    app_snake_open       },
    { "Tasks",    app_taskmgr_open     }
};
#define NUM_DOCK_ITEMS (int)(sizeof(s_dock_items) / sizeof(s_dock_items[0]))

void dock_init(void) {
}

/* Renders mini glyph for each dock icon slot */
static void draw_dock_icon_glyph(int idx, int cx, int cy) {
    switch (idx) {
        case 0: /* Terminal */
            gfx_fill_rect(cx - 12, cy - 12, 24, 24, 0x0011111B);
            gfx_draw_rect(cx - 12, cy - 12, 24, 24, 0x00A6E3A1);
            gfx_draw_char(cx - 6, cy - 8, '>', 0x00A6E3A1, 0, true);
            gfx_fill_rect(cx + 2, cy + 3, 5, 2, 0x00A6E3A1);
            break;

        case 1: /* Files */
            gfx_fill_rect(cx - 12, cy - 11, 11, 5, 0x00D49B00);
            gfx_fill_rect(cx - 12, cy - 7, 24, 18, 0x00F5BA13);
            gfx_draw_rect(cx - 12, cy - 7, 24, 18, 0x00A06D00);
            gfx_fill_rect(cx - 8, cy - 9, 12, 4, 0x00FFFFFF);
            break;

        case 2: /* Text Editor */
            gfx_fill_rect(cx - 10, cy - 13, 20, 26, 0x00F5F5F5);
            gfx_draw_rect(cx - 10, cy - 13, 20, 26, 0x00506070);
            for (int i = 0; i < 3; i++) {
                gfx_fill_rect(cx - 7, cy - 5 + i * 5, 14, 2, 0x0089B4FA);
            }
            break;

        case 3: /* Calculator */
            gfx_fill_rect(cx - 11, cy - 13, 22, 26, 0x00313244);
            gfx_draw_rect(cx - 11, cy - 13, 22, 26, 0x00B4BEFE);
            gfx_fill_rect(cx - 8, cy - 10, 16, 6, 0x00A6E3A1);
            for (int r = 0; r < 2; r++) {
                for (int c = 0; c < 3; c++) {
                    gfx_fill_rect(cx - 8 + c * 6, cy - 1 + r * 5, 4, 3, 0x00CDD6F4);
                }
            }
            break;

        case 4: /* Spreadsheet */
            gfx_fill_rect(cx - 11, cy - 12, 22, 24, 0x00FFFFFF);
            gfx_draw_rect(cx - 11, cy - 12, 22, 24, 0x001F6E43);
            gfx_fill_rect(cx - 11, cy - 12, 22, 7, 0x001F6E43);
            gfx_draw_line(cx - 11, cy + 2, cx + 10, cy + 2, 0x00A0C0A0);
            gfx_draw_line(cx, cy - 5, cx, cy + 11, 0x00A0C0A0);
            break;

        case 5: /* System Settings */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x00313244);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00CBA6F7);
            gfx_fill_rect(cx - 7, cy - 4, 14, 2, 0x00181825);
            gfx_fill_rect(cx - 7, cy + 4, 14, 2, 0x00181825);
            gfx_fill_rect(cx - 3, cy - 6, 4, 6, 0x0089B4FA);
            gfx_fill_rect(cx + 1, cy + 2, 4, 6, 0x00CBA6F7);
            break;

        case 6: /* Photos Viewer */
            gfx_fill_rect(cx - 11, cy - 10, 22, 20, 0x002B303C);
            gfx_draw_rect(cx - 11, cy - 10, 22, 20, 0x0089DCEB);
            gfx_fill_rect(cx - 7, cy - 6, 4, 4, 0x00F9E2AF); /* Sun */
            gfx_fill_rect(cx - 8, cy + 2, 10, 6, 0x00A6E3A1); /* Mountain 1 */
            gfx_fill_rect(cx - 1, cy - 1, 9, 9, 0x0094E2D5);  /* Mountain 2 */
            break;

        case 7: /* Music / Media Player */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x00181825);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00F5C2E7);
            /* Equalizer bar mini glyphs */
            gfx_fill_rect(cx - 7, cy - 4, 3, 12, 0x00A6E3A1);
            gfx_fill_rect(cx - 2, cy - 8, 3, 16, 0x00F9E2AF);
            gfx_fill_rect(cx + 3, cy - 1, 3, 9, 0x00F38BA8);
            break;

        case 8: /* Breakout Arcade */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x000F121C);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00FAB387);
            gfx_fill_rect(cx - 8, cy - 8, 5, 2, 0x00F38BA8); /* Red brick */
            gfx_fill_rect(cx - 1, cy - 8, 5, 2, 0x00FAB387); /* Orange */
            gfx_fill_rect(cx - 8, cy - 4, 5, 2, 0x00A6E3A1); /* Green */
            gfx_fill_rect(cx - 1, cy - 4, 5, 2, 0x00F9E2AF); /* Yellow */
            gfx_fill_rect(cx - 6, cy + 6, 12, 3, 0x0089B4FA); /* Paddle */
            gfx_fill_rect(cx + 1, cy + 1, 2, 2, 0x00FFFFFF); /* Ball */
            break;

        case 9: /* Minesweeper */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x00C0C0C0);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00707070);
            gfx_fill_rect(cx - 4, cy - 4, 8, 8, 0x0011111B);
            gfx_draw_line(cx - 7, cy, cx + 7, cy, 0x0011111B);
            gfx_draw_line(cx, cy - 7, cx, cy + 7, 0x0011111B);
            gfx_fill_rect(cx - 2, cy - 2, 2, 2, 0x00FFFFFF);
            break;

        case 10: /* Snake */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x00182818);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00336633);
            gfx_fill_rect(cx - 7, cy - 5, 6, 6, 0x00A6E3A1);
            gfx_fill_rect(cx - 1, cy - 5, 6, 6, 0x00A6E3A1);
            gfx_fill_rect(cx - 1, cy + 1, 6, 6, 0x00A6E3A1);
            gfx_fill_rect(cx + 3, cy - 4, 4, 4, 0x00F38BA8);
            break;

        case 11: /* Task Manager */
            gfx_fill_rect(cx - 11, cy - 11, 22, 22, 0x0011111B);
            gfx_draw_rect(cx - 11, cy - 11, 22, 22, 0x00F38BA8);
            gfx_draw_line(cx - 8, cy + 4, cx - 4, cy - 3, 0x00A6E3A1);
            gfx_draw_line(cx - 4, cy - 3, cx, cy + 2, 0x00A6E3A1);
            gfx_draw_line(cx, cy + 2, cx + 4, cy - 6, 0x00A6E3A1);
            gfx_draw_line(cx + 4, cy - 6, cx + 8, cy + 4, 0x00A6E3A1);
            break;

        default:
            gfx_fill_rect(cx - 10, cy - 10, 20, 20, 0x00808080);
            break;
    }
}

/*
 * dock_render:
 * Renders the floating pill dock.
 */
void dock_render(int clip_x, int clip_y, int clip_w, int clip_h) {
    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    int dx = DOCK_X;
    int dy = DOCK_Y;
    int dw = DOCK_WIDTH;
    int dh = DOCK_HEIGHT;

    if (dx + dw <= clip_x || dx >= clip_r || dy + dh <= clip_y || dy >= clip_b) {
        return;
    }

    const theme_t* theme = theme_get_current();
    uint32_t dock_bg     = theme ? theme->dock_bg : 0x00181825;
    uint32_t dock_border = theme ? theme->dock_border : 0x00CBA6F7;
    uint32_t accent_pri  = theme ? theme->accent_primary : 0x0089B4FA;

    /* 1. Pill Background with subtle beveled corner notches */
    gfx_fill_rect(dx + 6, dy, dw - 12, dh, dock_bg);
    gfx_fill_rect(dx, dy + 6, dw, dh - 12, dock_bg);

    /* 2. Beveled Pill Border */
    gfx_draw_line(dx + 6, dy, dx + dw - 7, dy, dock_border);
    gfx_draw_line(dx + 6, dy + dh - 1, dx + dw - 7, dy + dh - 1, dock_border);
    gfx_draw_line(dx, dy + 6, dx, dy + dh - 7, dock_border);
    gfx_draw_line(dx + dw - 1, dy + 6, dx + dw - 1, dy + dh - 7, dock_border);

    /* Corner diagonals */
    gfx_draw_line(dx, dy + 6, dx + 6, dy, dock_border);
    gfx_draw_line(dx + dw - 1, dy + 6, dx + dw - 7, dy, dock_border);
    gfx_draw_line(dx, dy + dh - 7, dx + 6, dy + dh - 1, dock_border);
    gfx_draw_line(dx + dw - 1, dy + dh - 7, dx + dw - 7, dy + dh - 1, dock_border);

    /* 3. Render App Icons */
    int item_spacing = 48;
    int start_x = dx + 26;

    for (int i = 0; i < NUM_DOCK_ITEMS; i++) {
        int icon_cx = start_x + i * item_spacing;
        int icon_cy = dy + 24;

        draw_dock_icon_glyph(i, icon_cx, icon_cy);

        /* Indicator pip dot beneath active/running icon */
        gfx_fill_rect(icon_cx - 2, dy + dh - 8, 4, 3, accent_pri);
    }
}

/*
 * dock_handle_mouse:
 * Handles clicks on dock app icons.
 */
bool dock_handle_mouse(int mx, int my, bool left_pressed) {
    if (!left_pressed) return false;

    int dx = DOCK_X;
    int dy = DOCK_Y;
    int dw = DOCK_WIDTH;
    int dh = DOCK_HEIGHT;

    if (mx < dx || mx >= dx + dw || my < dy || my >= dy + dh) {
        return false;
    }

    int item_spacing = 48;
    int start_x = dx + 26;

    for (int i = 0; i < NUM_DOCK_ITEMS; i++) {
        int icon_cx = start_x + i * item_spacing;
        if (mx >= icon_cx - 20 && mx <= icon_cx + 20) {
            if (s_dock_items[i].launch_fn) {
                s_dock_items[i].launch_fn();
            }
            return true;
        }
    }

    return true; /* Consumed dock click */
}
