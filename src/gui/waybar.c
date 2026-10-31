#include "gui/waybar.h"
#include "gui/theme.h"
#include "gui/startmenu.h"
#include "gui/lockscreen.h"
#include "apps/clock.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "proc/task.h"
#include "lib/string.h"

static int s_active_workspace = 1;

/* Helper: formats uint32_t into decimal string */
static void waybar_fmt_uint(uint32_t val, char* buf) {
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

void waybar_init(void) {
    s_active_workspace = 1;
}

int waybar_get_active_workspace(void) {
    return s_active_workspace;
}

void waybar_set_active_workspace(int ws) {
    if (ws >= 1 && ws <= 3) {
        s_active_workspace = ws;
    }
}

/*
 * waybar_render:
 * Renders the top Waybar status bar.
 */
void waybar_render(int clip_x, int clip_y, int clip_w, int clip_h) {
    (void)clip_x;
    (void)clip_w;
    int clip_b = clip_y + clip_h;

    /* Check intersection with top 28px */
    if (clip_y >= WAYBAR_HEIGHT || clip_b <= 0) {
        return;
    }

    const theme_t* theme = theme_get_current();
    uint32_t bar_bg     = theme ? theme->bg_panel : 0x00181825;
    uint32_t bar_border = theme ? theme->border_active : 0x00CBA6F7;
    uint32_t text_pri   = theme ? theme->text_primary : 0x00CDD6F4;
    uint32_t text_mut   = theme ? theme->text_secondary : 0x00A6ADC8;
    uint32_t accent_pri = theme ? theme->accent_primary : 0x0089B4FA;
    uint32_t accent_sec = theme ? theme->accent_secondary : 0x00A6E3A1;
    uint32_t accent_dan = theme ? theme->accent_danger : 0x00F38BA8;

    /* 1. Bar Background */
    gfx_fill_rect(0, 0, 1024, WAYBAR_HEIGHT, bar_bg);
    gfx_draw_line(0, WAYBAR_HEIGHT - 1, 1023, WAYBAR_HEIGHT - 1, bar_border);

    /* 2. Workspace Pills (Left Side) */
    const char* ws_labels[3] = { "1", "2", "3" };
    int ws_x = 10;
    for (int i = 0; i < 3; i++) {
        int px = ws_x + i * 36;
        int py = 4;
        int pw = 30;
        int ph = 20;

        bool active = (s_active_workspace == (i + 1));
        if (active) {
            gfx_fill_rect(px, py, pw, ph, accent_pri);
            gfx_draw_rect(px, py, pw, ph, bar_border);
            gfx_draw_string(px + 11, py + 2, ws_labels[i], bar_bg, accent_pri, true);
        } else {
            gfx_fill_rect(px, py, pw, ph, bar_bg);
            gfx_draw_rect(px, py, pw, ph, text_mut);
            gfx_draw_string(px + 11, py + 2, ws_labels[i], text_mut, bar_bg, true);
        }
    }

    /* 3. Start Menu Chip [ MyOS ] */
    int start_x = 124;
    int start_w = 68;
    gfx_fill_rect(start_x, 4, start_w, 20, bar_bg);
    gfx_draw_rect(start_x, 4, start_w, 20, bar_border);
    gfx_draw_string(start_x + 10, 6, "MyOS", accent_pri, bar_bg, true);

    /* 4. Centered Brand Chip */
    int center_w = 260;
    int center_x = (1024 - center_w) / 2;
    int center_y = 4;
    int center_h = 20;

    gfx_fill_rect(center_x, center_y, center_w, center_h, bar_bg);
    gfx_draw_rect(center_x, center_y, center_w, center_h, text_mut);
    gfx_draw_string(center_x + 16, center_y + 2, "MyOS • x86 Protected Mode", text_pri, bar_bg, true);

    /* 4. Right Telemetry Chips */
    /* A. RAM Chip */
    uint32_t total_f = 32768;
    uint32_t free_f = pmm_free_frames();
    uint32_t used_f = (total_f > free_f) ? (total_f - free_f) : 0;
    int ram_pct = (int)((used_f * 100) / total_f);

    char ram_chip[24] = "RAM: ";
    char pct_buf[8];
    waybar_fmt_uint((uint32_t)ram_pct, pct_buf);
    strlcat(ram_chip, pct_buf, sizeof(ram_chip));
    strlcat(ram_chip, "%", sizeof(ram_chip));

    int ram_x = 730;
    gfx_fill_rect(ram_x, 4, 82, 20, bar_bg);
    gfx_draw_rect(ram_x, 4, 82, 20, accent_sec);
    gfx_draw_string(ram_x + 8, 6, ram_chip, accent_sec, bar_bg, true);

    /* B. Task Count Chip */
    uint32_t tasks = task_get_count();
    char task_chip[20] = "Tasks: ";
    char t_buf[8];
    waybar_fmt_uint(tasks, t_buf);
    strlcat(task_chip, t_buf, sizeof(task_chip));

    int task_x = 818;
    gfx_fill_rect(task_x, 4, 80, 20, bar_bg);
    gfx_draw_rect(task_x, 4, 80, 20, text_mut);
    gfx_draw_string(task_x + 8, 6, task_chip, text_pri, bar_bg, true);

    /* C. Lock Screen Button */
    int lock_x = 904;
    gfx_fill_rect(lock_x, 4, 50, 20, bar_bg);
    gfx_draw_rect(lock_x, 4, 50, 20, accent_dan);
    gfx_draw_string(lock_x + 9, 6, "Lock", accent_dan, bar_bg, true);

    /* D. Clock Chip */
    char clock_buf[16];
    timer_format_clock(clock_buf);
    char hm_buf[6];
    hm_buf[0] = clock_buf[0];
    hm_buf[1] = clock_buf[1];
    hm_buf[2] = clock_buf[2];
    hm_buf[3] = clock_buf[3];
    hm_buf[4] = clock_buf[4];
    hm_buf[5] = '\0';

    int clk_x = 960;
    gfx_fill_rect(clk_x, 4, 56, 20, bar_bg);
    gfx_draw_rect(clk_x, 4, 56, 20, bar_border);
    gfx_draw_string(clk_x + 8, 6, hm_buf, text_pri, bar_bg, true);
}

/*
 * waybar_handle_mouse:
 * Processes mouse clicks on the top bar.
 */
bool waybar_handle_mouse(int mx, int my, bool left_pressed) {
    if (!left_pressed) return false;
    if (my < 0 || my >= WAYBAR_HEIGHT) return false;

    /* Workspace 1, 2, 3 clicks */
    for (int i = 0; i < 3; i++) {
        int px = 10 + i * 36;
        if (mx >= px && mx < px + 30 && my >= 4 && my < 24) {
            s_active_workspace = i + 1;
            /* Invalidate top bar */
            gfx_flush_rect(0, 0, 1024, WAYBAR_HEIGHT);
            return true;
        }
    }

    /* Start Menu Chip Click (x: 124 to 194) */
    if (mx >= 124 && mx < 194 && my >= 4 && my < 24) {
        startmenu_toggle();
        return true;
    }

    /* Centered Brand Chip Click (x: 382 to 642) also opens Start Menu */
    if (mx >= 382 && mx < 642 && my >= 4 && my < 24) {
        startmenu_toggle();
        return true;
    }

    /* Lock Button Click (x: 904 to 954) */
    if (mx >= 904 && mx < 954 && my >= 4 && my < 24) {
        lockscreen_activate();
        return true;
    }

    /* Clock Chip Click (x: 960 to 1016) opens Clock & Calendar app */
    if (mx >= 960 && mx < 1016 && my >= 4 && my < 24) {
        app_clock_open();
        return true;
    }

    return true; /* Consumed top bar click */
}
