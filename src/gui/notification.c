#include "gui/notification.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "lib/string.h"

#define TOAST_W 240
#define TOAST_H 54
#define TOAST_Y 36
#define TOAST_INITIAL_TICKS 350

static notification_t s_active_toast;
static notification_t s_history[NOTIFY_MAX_HISTORY];
static int s_history_count = 0;

void notification_init(void) {
    s_active_toast.active = false;
    s_active_toast.ticks_remaining = 0;
    s_history_count = 0;
}

void notification_push(notify_type_t type, const char* title, const char* message) {
    s_active_toast.type = type;
    s_active_toast.ticks_remaining = TOAST_INITIAL_TICKS;
    s_active_toast.active = true;

    strlcpy(s_active_toast.title, title ? title : "Notification", sizeof(s_active_toast.title));
    strlcpy(s_active_toast.message, message ? message : "", sizeof(s_active_toast.message));

    /* Add to rolling history */
    if (s_history_count < NOTIFY_MAX_HISTORY) {
        s_history[s_history_count++] = s_active_toast;
    } else {
        for (int i = 0; i < NOTIFY_MAX_HISTORY - 1; i++) {
            s_history[i] = s_history[i + 1];
        }
        s_history[NOTIFY_MAX_HISTORY - 1] = s_active_toast;
    }

    /* Invalidate toast region on screen */
    wm_invalidate_rect(1024 - TOAST_W - 12, TOAST_Y, TOAST_W, TOAST_H);
}

void notification_tick(void) {
    if (s_active_toast.active) {
        if (s_active_toast.ticks_remaining > 0) {
            s_active_toast.ticks_remaining--;
            if ((s_active_toast.ticks_remaining % 10) == 0) {
                wm_invalidate_rect(1024 - TOAST_W - 12, TOAST_Y, TOAST_W, TOAST_H);
            }
        } else {
            s_active_toast.active = false;
            wm_invalidate_rect(1024 - TOAST_W - 12, TOAST_Y, TOAST_W, TOAST_H);
        }
    }
}

bool notification_handle_click(int32_t mx, int32_t my) {
    if (!s_active_toast.active) {
        return false;
    }
    int toast_x = 1024 - TOAST_W - 12;
    if (mx >= toast_x && mx < toast_x + TOAST_W &&
        my >= TOAST_Y && my < TOAST_Y + TOAST_H) {
        s_active_toast.active = false;
        return true;
    }
    return false;
}

void notification_render(uint32_t* backbuf, uint32_t fb_width, uint32_t fb_height, const theme_t* theme) {
    (void)backbuf;
    (void)fb_height;
    if (!s_active_toast.active) {
        return;
    }

    int x = (int)fb_width - TOAST_W - 12;
    int y = TOAST_Y;

    uint32_t accent_color;
    const char* badge_text;
    switch (s_active_toast.type) {
        case NOTIFY_SUCCESS:
            accent_color = 0x00A6E3A1; /* Green */
            badge_text = "OK";
            break;
        case NOTIFY_WARNING:
            accent_color = 0x00F9E2AF; /* Yellow */
            badge_text = "WARN";
            break;
        case NOTIFY_SECURITY:
            accent_color = 0x00F38BA8; /* Red/Pink */
            badge_text = "SEC";
            break;
        case NOTIFY_INFO:
        default:
            accent_color = theme ? theme->accent_primary : 0x00CBA6F7; /* Mauve */
            badge_text = "INFO";
            break;
    }

    /* Card background & border */
    uint32_t card_bg = theme ? theme->bg_card : 0x00181825;
    uint32_t border_col = theme ? theme->border_active : 0x0045475A;
    gfx_fill_rect(x, y, TOAST_W, TOAST_H, card_bg);
    gfx_draw_rect(x, y, TOAST_W, TOAST_H, border_col);

    /* Left accent vertical indicator strip */
    gfx_fill_rect(x, y, 5, TOAST_H, accent_color);

    /* Type badge */
    gfx_fill_rect(x + 10, y + 8, 36, 14, accent_color);
    gfx_draw_string(x + 12, y + 8, badge_text, 0x0011111B, 0, true);

    /* Title text */
    uint32_t title_col = theme ? theme->text_primary : 0x00CDD6F4;
    gfx_draw_string(x + 52, y + 8, s_active_toast.title, title_col, 0, true);

    /* Message text */
    uint32_t sub_col = theme ? theme->text_secondary : 0x00A6ADC8;
    gfx_draw_string(x + 12, y + 26, s_active_toast.message, sub_col, 0, true);

    /* Bottom remaining progress bar */
    if (TOAST_INITIAL_TICKS > 0) {
        int bar_w = ((int)s_active_toast.ticks_remaining * (TOAST_W - 10)) / TOAST_INITIAL_TICKS;
        if (bar_w > 0) {
            gfx_fill_rect(x + 5, y + TOAST_H - 3, bar_w, 2, accent_color);
        }
    }
}
