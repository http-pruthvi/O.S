#include "gui/lockscreen.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"

static bool s_locked = false;
static char s_pin_buf[16];
static int  s_pin_len = 0;
static bool s_wrong_pin = false;

void lockscreen_init(void) {
    s_locked = false;
    s_pin_len = 0;
    s_wrong_pin = false;
    memset(s_pin_buf, 0, sizeof(s_pin_buf));
}

void lockscreen_activate(void) {
    s_locked = true;
    s_pin_len = 0;
    s_wrong_pin = false;
    memset(s_pin_buf, 0, sizeof(s_pin_buf));
    serial_writestring("[LOCK] System locked. Hyprlock authentication active.\n");
    lockscreen_render();
    gfx_flush();
}

bool lockscreen_is_active(void) {
    return s_locked;
}

/*
 * lockscreen_render:
 * Draws the centered Hyprlock modal card over a dimmed backdrop.
 */
void lockscreen_render(void) {
    if (!s_locked) return;

    uint32_t* backbuf = gfx_get_backbuffer();
    if (!backbuf) return;

    /* 1. Dim entire 1024x768 backdrop by 50% using bitwise masking */
    size_t total_pixels = 1024 * 768;
    for (size_t i = 0; i < total_pixels; i++) {
        backbuf[i] = (backbuf[i] & 0x00FEFEFE) >> 1;
    }

    const theme_t* theme = theme_get_current();
    uint32_t card_bg     = theme ? theme->bg_panel : 0x00181825;
    uint32_t card_border = s_wrong_pin ? (theme ? theme->accent_danger : 0x00F38BA8)
                                       : (theme ? theme->border_active : 0x00CBA6F7);
    uint32_t text_pri    = theme ? theme->text_primary : 0x00CDD6F4;
    uint32_t text_mut    = theme ? theme->text_secondary : 0x00A6ADC8;
    uint32_t accent_pri  = theme ? theme->accent_primary : 0x0089B4FA;
    uint32_t accent_dan  = theme ? theme->accent_danger : 0x00F38BA8;

    /* 2. Centered Card Dimensions: 360 x 340 */
    int card_w = 360;
    int card_h = 340;
    int card_x = (1024 - card_w) / 2;
    int card_y = (768 - card_h) / 2;

    /* Card background & outline */
    gfx_fill_rect(card_x, card_y, card_w, card_h, card_bg);
    gfx_draw_rect(card_x, card_y, card_w, card_h, card_border);

    /* Decorative top accent stripe */
    gfx_fill_rect(card_x + 1, card_y + 1, card_w - 2, 4, card_border);

    /* 3. Circular Avatar Monogram Badge [ PV ] */
    int av_cx = 512;
    int av_cy = card_y + 54;
    int av_r  = 28;

    /* Pseudo-circle badge */
    gfx_fill_rect(av_cx - av_r + 4, av_cy - av_r, (av_r - 4) * 2, av_r * 2, accent_pri);
    gfx_fill_rect(av_cx - av_r, av_cy - av_r + 4, av_r * 2, (av_r - 4) * 2, accent_pri);
    gfx_draw_rect(av_cx - av_r + 4, av_cy - av_r, (av_r - 4) * 2, av_r * 2, card_border);

    /* Monogram initials inside avatar badge */
    gfx_draw_string(av_cx - 16, av_cy - 8, "[PV]", 0x0011111B, accent_pri, true);

    /* 4. User Name & Status Subtitle */
    const char* user_name = "Pruthviraj (Admin)";
    int un_len = (int)strlen(user_name);
    int un_x = 512 - (un_len * FONT_GLYPH_WIDTH) / 2;
    gfx_draw_string(un_x, card_y + 98, user_name, text_pri, card_bg, true);

    const char* sub_txt = "MyOS Workstation Authenticator";
    int sub_len = (int)strlen(sub_txt);
    int sub_x = 512 - (sub_len * FONT_GLYPH_WIDTH) / 2;
    gfx_draw_string(sub_x, card_y + 118, sub_txt, text_mut, card_bg, true);

    /* 5. Large 2x Typographic Digital Clock */
    char clock_buf[16];
    timer_format_clock(clock_buf);
    char hm_buf[6];
    hm_buf[0] = clock_buf[0];
    hm_buf[1] = clock_buf[1];
    hm_buf[2] = clock_buf[2];
    hm_buf[3] = clock_buf[3];
    hm_buf[4] = clock_buf[4];
    hm_buf[5] = '\0';

    int clk_x = 512 - (5 * 16) / 2;
    gfx_draw_string_scaled(clk_x, card_y + 150, hm_buf, 2, text_pri, card_bg, true);

    /* 6. PIN Input Box */
    int pin_box_w = 200;
    int pin_box_h = 32;
    int pin_box_x = 512 - pin_box_w / 2;
    int pin_box_y = card_y + 204;

    gfx_fill_rect(pin_box_x, pin_box_y, pin_box_w, pin_box_h, 0x0011111B);
    gfx_draw_rect(pin_box_x, pin_box_y, pin_box_w, pin_box_h, card_border);

    if (s_wrong_pin) {
        gfx_draw_string(pin_box_x + 44, pin_box_y + 8, "Incorrect PIN!", accent_dan, 0x0011111B, true);
    } else if (s_pin_len == 0) {
        gfx_draw_string(pin_box_x + 36, pin_box_y + 8, "Enter PIN (1234)", text_mut, 0x0011111B, true);
    } else {
        /* Draw masked bullet dots for each character */
        int dot_spacing = 16;
        int dots_w = s_pin_len * dot_spacing;
        int dots_start = 512 - dots_w / 2;
        for (int i = 0; i < s_pin_len; i++) {
            int dot_x = dots_start + i * dot_spacing;
            int dot_y = pin_box_y + 12;
            gfx_fill_rect(dot_x + 2, dot_y, 8, 8, accent_pri);
        }
    }

    /* 7. Action Hint */
    const char* hint_txt = "Press [ENTER] to unlock session";
    int hint_len = (int)strlen(hint_txt);
    int hint_x = 512 - (hint_len * FONT_GLYPH_WIDTH) / 2;
    gfx_draw_string(hint_x, card_y + 258, hint_txt, text_mut, card_bg, true);
}

/*
 * lockscreen_handle_key:
 * Dispatches keystroke to lock screen PIN verification.
 */
bool lockscreen_handle_key(char c) {
    if (!s_locked) return false;

    /* Enter key submits PIN */
    if (c == '\n' || c == '\r') {
        /* Check if PIN matches '1234' or allow instant unlock if blank */
        if (s_pin_len == 0 || strcmp(s_pin_buf, "1234") == 0) {
            /* Unlock success! */
            s_locked = false;
            memset(s_pin_buf, 0, sizeof(s_pin_buf));
            s_pin_len = 0;
            s_wrong_pin = false;
            serial_writestring("[LOCK] Authentication successful. Session restored.\n");
            wm_redraw_all();
            return true;
        } else {
            /* Incorrect PIN */
            s_wrong_pin = true;
            memset(s_pin_buf, 0, sizeof(s_pin_buf));
            s_pin_len = 0;
            serial_writestring("[LOCK] Authentication failed: invalid PIN.\n");
            lockscreen_render();
            gfx_flush();
            return false;
        }
    }

    /* Backspace removes character */
    if (c == '\b') {
        if (s_pin_len > 0) {
            s_pin_len--;
            s_pin_buf[s_pin_len] = '\0';
            s_wrong_pin = false;
            lockscreen_render();
            gfx_flush();
        }
        return false;
    }

    /* Printable characters append to PIN */
    if (c >= 32 && c <= 126) {
        if (s_pin_len < (int)sizeof(s_pin_buf) - 1) {
            s_wrong_pin = false;
            s_pin_buf[s_pin_len++] = c;
            s_pin_buf[s_pin_len] = '\0';
            lockscreen_render();
            gfx_flush();
        }
        return false;
    }

    return false;
}
