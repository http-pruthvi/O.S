#include "gui/notification.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "lib/string.h"

#define TOAST_W 240
#define TOAST_H 54
#define TOAST_INITIAL_TICKS 350

static inline int notification_get_y(void) {
    if (theme_get_current_id() == THEME_WINDOWS_11) {
        return WM_TASKBAR_Y - TOAST_H - 10;
    }
    return 36;
}

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
    int toast_y = notification_get_y();
    wm_invalidate_rect(1024 - TOAST_W - 12, toast_y, TOAST_W, TOAST_H);
}

void notification_tick(void) {
    if (s_active_toast.active) {
        int toast_y = notification_get_y();
        if (s_active_toast.ticks_remaining > 0) {
            s_active_toast.ticks_remaining--;
            if ((s_active_toast.ticks_remaining % 10) == 0) {
                wm_invalidate_rect(1024 - TOAST_W - 12, toast_y, TOAST_W, TOAST_H);
            }
        } else {
            s_active_toast.active = false;
            wm_invalidate_rect(1024 - TOAST_W - 12, toast_y, TOAST_W, TOAST_H);
        }
    }
}

bool notification_handle_click(int32_t mx, int32_t my) {
    if (!s_active_toast.active) {
        return false;
    }
    int toast_x = 1024 - TOAST_W - 12;
    int toast_y = notification_get_y();
    if (mx >= toast_x && mx < toast_x + TOAST_W &&
        my >= toast_y && my < toast_y + TOAST_H) {
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
    int y = notification_get_y();

    uint32_t accent_color;
    const char* badge_text;
    switch (s_active_toast.type) {
        case NOTIFY_SUCCESS:
            accent_color = 0x0034D399; /* Emerald Green */
            badge_text = "OK";
            break;
        case NOTIFY_WARNING:
            accent_color = 0x00FBBF24; /* Amber */
            badge_text = "WARN";
            break;
        case NOTIFY_SECURITY:
            accent_color = 0x00F87171; /* Coral Red */
            badge_text = "SEC";
            break;
        case NOTIFY_INFO:
        default:
            accent_color = theme ? theme->accent_primary : 0x000078D4; /* Fluent Accent Blue */
            badge_text = "INFO";
            break;
    }

    uint32_t bg_card = theme ? theme->bg_card : 0x001F1F1F;
    uint32_t border_col = theme ? theme->border_active : 0x000078D4;

    /* Card Shadow / Frame */
    gfx_fill_rect(x, y, TOAST_W, TOAST_H, bg_card);
    gfx_draw_rect(x, y, TOAST_W, TOAST_H, border_col);

    /* Left Accent Status Strip (4px width) */
    gfx_fill_rect(x, y, 4, TOAST_H, accent_color);

    /* Category Badge */
    int badge_x = x + 10;
    int badge_y = y + 8;
    int badge_w = (int)strlen(badge_text) * 8 + 8;
    gfx_fill_rect(badge_x, badge_y, badge_w, 16, accent_color);
    gfx_draw_string(badge_x + 4, badge_y, badge_text, 0x0011111B, 0, true);

    /* Title Text */
    gfx_draw_string(badge_x + badge_w + 8, badge_y, s_active_toast.title, 0x00FFFFFF, 0, true);

    /* Message Body */
    gfx_draw_string(x + 10, y + 28, s_active_toast.message, 0x00CDD6F4, 0, true);

    /* Subtle Animated Progress / Decay Bar at Bottom of Card */
    int bar_w = ((TOAST_W - 2) * s_active_toast.ticks_remaining) / TOAST_INITIAL_TICKS;
    if (bar_w > 0) {
        gfx_fill_rect(x + 1, y + TOAST_H - 2, bar_w, 2, accent_color);
    }
}
