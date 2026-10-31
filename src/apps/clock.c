#include "apps/clock.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define CLOCK_WIN_WIDTH   320
#define CLOCK_WIN_HEIGHT  350

/* Clock & Calendar State */
static window_t* s_clock_win  = NULL;
static widget_t* s_lbl_month  = NULL;

/* Current Calendar View Date (Initial: October 2026) */
static int s_cal_year  = 2026;
static int s_cal_month = 10;
static int s_cal_today = 31;

static const char* s_month_names[] = {
    "", "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};

static const int s_days_in_month[] = {
    0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* Checks if year is a leap year */
static bool is_leap_year(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/* Returns number of days in specified month/year */
static int get_days_in_month(int y, int m) {
    if (m == 2 && is_leap_year(y)) return 29;
    if (m >= 1 && m <= 12) return s_days_in_month[m];
    return 30;
}

/*
 * get_first_day_of_week:
 * Returns day of week (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
 * for the 1st of month/year using Sakamoto's algorithm.
 */
static int get_first_day_of_week(int year, int month) {
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (month < 3) year -= 1;
    return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + 1) % 7;
}

/* Helper: converts integer to string */
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

/*
 * render_digital_clock:
 * Renders the top digital clock readout and uptime statistics.
 */
static void render_digital_clock(void) {
    if (!s_clock_win) return;

    /* 1. Readout background box */
    window_fill_rect(s_clock_win, 14, 10, 288, 56, 0x000F1E36);
    window_draw_rect(s_clock_win, 14, 10, 288, 56, 0x003A5070);

    /* 2. Format Time String */
    char clock_buf[16];
    timer_format_clock(clock_buf);

    /* Time readout centered */
    int time_len = (int)strlen(clock_buf);
    int time_x = 14 + (288 - time_len * FONT_GLYPH_WIDTH) / 2;
    window_draw_string(s_clock_win, time_x, 18, clock_buf, 0x0000FFFF, 0x000F1E36, true);

    /* 3. System Uptime */
    uint32_t uptime_sec = timer_get_uptime_sec();
    char uptime_buf[48] = "System Uptime: ";
    int idx = 15;
    int_to_str((int)uptime_sec, uptime_buf + idx);
    while (uptime_buf[idx]) idx++;
    uptime_buf[idx++] = ' '; uptime_buf[idx++] = 's'; uptime_buf[idx++] = 'e'; uptime_buf[idx++] = 'c';
    uptime_buf[idx] = '\0';

    int up_len = (int)strlen(uptime_buf);
    int up_x = 14 + (288 - up_len * FONT_GLYPH_WIDTH) / 2;
    window_draw_string(s_clock_win, up_x, 40, uptime_buf, 0x0090A8C0, 0x000F1E36, true);

    /* Invalidate top readout */
    wm_invalidate_rect(s_clock_win->client_x + 14, s_clock_win->client_y + 10, 288, 56);
}

/*
 * render_calendar_grid:
 * Renders the monthly calendar grid and date numbers.
 */
static void render_calendar_grid(void) {
    if (!s_clock_win) return;

    /* Update month label: e.g. "October 2026" */
    char month_buf[32];
    strcpy(month_buf, s_month_names[s_cal_month]);
    int m_len = (int)strlen(month_buf);
    month_buf[m_len++] = ' ';
    int_to_str(s_cal_year, month_buf + m_len);
    if (s_lbl_month) {
        widget_set_text(s_lbl_month, month_buf);
    }

    int gx = 16;
    int gy = 114;
    int cell_w = 40;
    int cell_h = 24;

    /* 1. Day of Week Header: Su Mo Tu We Th Fr Sa */
    static const char* dow_names[] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    for (int d = 0; d < 7; d++) {
        int cx = gx + d * cell_w;
        window_fill_rect(s_clock_win, cx, gy, cell_w, 20, 0x00E0E6F0);
        window_draw_rect(s_clock_win, cx, gy, cell_w, 20, 0x00B0C0D0);
        window_draw_string(s_clock_win, cx + (cell_w - 16) / 2, gy + 3,
                           dow_names[d], (d == 0 || d == 6) ? 0x00A02020 : 0x00203040, 0x00E0E6F0, true);
    }

    /* 2. Calendar Month Matrix (Up to 6 rows) */
    int start_dow = get_first_day_of_week(s_cal_year, s_cal_month);
    int total_days = get_days_in_month(s_cal_year, s_cal_month);

    int cur_day = 1;
    for (int row = 0; row < 6; row++) {
        int ry = gy + 22 + row * cell_h;

        for (int col = 0; col < 7; col++) {
            int cx = gx + col * cell_w;

            /* Check if cell has a day number */
            if ((row == 0 && col < start_dow) || cur_day > total_days) {
                /* Empty cell */
                window_fill_rect(s_clock_win, cx, ry, cell_w, cell_h, 0x00F4F4F4);
                window_draw_rect(s_clock_win, cx, ry, cell_w, cell_h, 0x00E0E0E0);
            } else {
                /* Active day cell */
                bool is_today = (s_cal_year == 2026 && s_cal_month == 10 && cur_day == s_cal_today);
                uint32_t bg = is_today ? 0x00D0E4FF : 0x00FFFFFF;

                window_fill_rect(s_clock_win, cx, ry, cell_w, cell_h, bg);
                window_draw_rect(s_clock_win, cx, ry, cell_w, cell_h, is_today ? 0x002060C0 : 0x00D0D0D0);

                char day_buf[4];
                int_to_str(cur_day, day_buf);
                int d_len = (int)strlen(day_buf);
                int tx = cx + (cell_w - d_len * FONT_GLYPH_WIDTH) / 2;

                uint32_t fg = is_today ? 0x00103090 : ((col == 0 || col == 6) ? 0x00902020 : 0x00101010);
                window_draw_string(s_clock_win, tx, ry + 4, day_buf, fg, bg, true);

                cur_day++;
            }
        }
    }

    /* Invalidate calendar area */
    wm_invalidate_rect(s_clock_win->client_x + gx, s_clock_win->client_y + gy, 7 * cell_w, 22 + 6 * cell_h);
}

/* Callback: Previous Month */
static void on_btn_prev_month(widget_t* w, void* data) {
    (void)w; (void)data;
    s_cal_month--;
    if (s_cal_month < 1) {
        s_cal_month = 12;
        s_cal_year--;
    }
    render_calendar_grid();
}

/* Callback: Next Month */
static void on_btn_next_month(widget_t* w, void* data) {
    (void)w; (void)data;
    s_cal_month++;
    if (s_cal_month > 12) {
        s_cal_month = 1;
        s_cal_year++;
    }
    render_calendar_grid();
}

/* Window Close Handler */
static void on_clock_close(window_t* win) {
    wm_remove_window(win);
    s_clock_win = NULL;
    s_lbl_month = NULL;
}

/*
 * app_clock_open:
 * Creates and displays the Clock & Calendar application window.
 */
void app_clock_open(void) {
    if (s_clock_win) {
        wm_bring_to_front(s_clock_win);
        return;
    }

    s_clock_win = window_create(640, 100, CLOCK_WIN_WIDTH, CLOCK_WIN_HEIGHT,
                                "Clock & Calendar", WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_clock_win) return;

    s_clock_win->on_close = on_clock_close;

    /* Month Navigation Controls at y = 78 */
    widget_add_to_window(s_clock_win, widget_create_button(16, 78, 48, 26, "<", on_btn_prev_month, NULL));

    s_lbl_month = widget_create_label(76, 84, 168, 20, "October 2026", 0x001F487E);
    widget_add_to_window(s_clock_win, s_lbl_month);

    widget_add_to_window(s_clock_win, widget_create_button(256, 78, 48, 26, ">", on_btn_next_month, NULL));

    /* Initial rendering of clock and calendar */
    render_digital_clock();
    render_calendar_grid();

    wm_add_window(s_clock_win);
    serial_writestring("[CLOCK] Clock & Calendar window opened.\n");
}

/*
 * app_clock_update:
 * Called periodically to update the digital clock readout.
 */
void app_clock_update(void) {
    if (s_clock_win && (s_clock_win->flags & WINDOW_FLAG_VISIBLE) &&
        !(s_clock_win->flags & WINDOW_FLAG_MINIMIZED)) {
        render_digital_clock();
    }
}
