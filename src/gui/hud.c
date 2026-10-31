#include "gui/hud.h"
#include "gui/theme.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "proc/task.h"
#include "lib/string.h"

/* HUD State */
static bool s_hud_enabled = true;

/* Helper: formats uint32_t to decimal string */
static void hud_fmt_uint(uint32_t val, char* buf) {
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

/* Helper to draw a horizontal progress meter bar */
static void draw_meter_bar(int x, int y, int w, int h, int percent, uint32_t fill_color, uint32_t bg_color, uint32_t border_color) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    gfx_fill_rect(x, y, w, h, bg_color);
    gfx_draw_rect(x, y, w, h, border_color);

    int fill_w = ((w - 4) * percent) / 100;
    if (fill_w > 0) {
        gfx_fill_rect(x + 2, y + 2, fill_w, h - 4, fill_color);
    }
}

void hud_init(void) {
    s_hud_enabled = true;
}

void hud_toggle(void) {
    s_hud_enabled = !s_hud_enabled;
}

bool hud_is_enabled(void) {
    return s_hud_enabled;
}

/*
 * hud_render:
 * Renders the floating Conky-style HUD cards onto the desktop background.
 */
void hud_render(int clip_x, int clip_y, int clip_w, int clip_h) {
    if (!s_hud_enabled) return;

    int clip_r = clip_x + clip_w;
    int clip_b = clip_y + clip_h;

    /* Retrieve current theme tokens */
    const theme_t* theme = theme_get_current();
    uint32_t card_bg     = theme ? theme->bg_card : 0x00181825;
    uint32_t card_border = theme ? theme->border_active : 0x00CBA6F7;
    uint32_t text_pri    = theme ? theme->text_primary : 0x00CDD6F4;
    uint32_t text_mut    = theme ? theme->text_secondary : 0x00A6ADC8;
    uint32_t accent_pri  = theme ? theme->accent_primary : 0x0089B4FA;
    uint32_t accent_sec  = theme ? theme->accent_secondary : 0x00A6E3A1;

    /* -------------------------------------------------------------
     * 1. Left Card: System & Telemetry HUD (x=24, y=48, w=280, h=194)
     * ------------------------------------------------------------- */
    int l_x = 24;
    int l_y = 48;
    int l_w = 280;
    int l_h = 194;

    if (!(l_x + l_w <= clip_x || l_x >= clip_r || l_y + l_h <= clip_y || l_y >= clip_b)) {
        /* Card Background & Frame */
        gfx_fill_rect(l_x, l_y, l_w, l_h, card_bg);
        gfx_draw_rect(l_x, l_y, l_w, l_h, card_border);

        /* Accent top stripe */
        gfx_fill_rect(l_x + 1, l_y + 1, l_w - 2, 3, card_border);

        /* Card Header */
        gfx_draw_string(l_x + 14, l_y + 12, "SYSTEM TELEMETRY HUD", accent_pri, card_bg, true);
        gfx_draw_line(l_x + 14, l_y + 30, l_x + l_w - 14, l_y + 30, text_mut);

        /* Architecture & Hardware specs */
        gfx_draw_string(l_x + 14, l_y + 38, "Arch:  x86 (i686 Protected)", text_pri, card_bg, true);
        gfx_draw_string(l_x + 14, l_y + 56, "Mode:  Flat 4GB Identity", text_pri, card_bg, true);

        /* Physical Memory (RAM) Calculation */
        uint32_t total_frames = 32768; /* 128 MB */
        uint32_t free_frames  = pmm_free_frames();
        uint32_t used_frames  = (total_frames > free_frames) ? (total_frames - free_frames) : 0;
        int ram_pct = (int)((used_frames * 100) / total_frames);
        uint32_t used_mb = (used_frames * 4) / 1024;

        char ram_str[48] = "RAM: ";
        char num[16];
        hud_fmt_uint(used_mb, num);
        strlcat(ram_str, num, sizeof(ram_str));
        strlcat(ram_str, " MB / 128 MB (", sizeof(ram_str));
        hud_fmt_uint((uint32_t)ram_pct, num);
        strlcat(ram_str, num, sizeof(ram_str));
        strlcat(ram_str, "%)", sizeof(ram_str));

        gfx_draw_string(l_x + 14, l_y + 78, ram_str, text_pri, card_bg, true);
        draw_meter_bar(l_x + 14, l_y + 96, l_w - 28, 8, ram_pct, accent_sec, 0x0011111B, card_border);

        /* Kernel Heap Allocation */
        size_t used_b = 0, free_b = 0, blocks = 0;
        heap_stats(&used_b, &free_b, &blocks);
        uint32_t heap_used_kb = (uint32_t)(used_b / 1024);
        int heap_pct = (int)((heap_used_kb * 100) / 32768); /* Out of 32 MB virtual heap max */
        if (heap_pct < 2) heap_pct = 2;

        char heap_str[48] = "Heap: ";
        hud_fmt_uint(heap_used_kb, num);
        strlcat(heap_str, num, sizeof(heap_str));
        strlcat(heap_str, " KB used (", sizeof(heap_str));
        hud_fmt_uint((uint32_t)blocks, num);
        strlcat(heap_str, num, sizeof(heap_str));
        strlcat(heap_str, " blks)", sizeof(heap_str));

        gfx_draw_string(l_x + 14, l_y + 112, heap_str, text_pri, card_bg, true);
        draw_meter_bar(l_x + 14, l_y + 130, l_w - 28, 8, heap_pct, card_border, 0x0011111B, card_border);

        /* Task Count & Storage Info */
        uint32_t task_cnt = task_get_count();
        char task_str[48] = "Tasks: ";
        hud_fmt_uint(task_cnt, num);
        strlcat(task_str, num, sizeof(task_str));
        strlcat(task_str, " active | RR: 100ms", sizeof(task_str));
        gfx_draw_string(l_x + 14, l_y + 148, task_str, text_mut, card_bg, true);

        gfx_draw_string(l_x + 14, l_y + 168, "VFS: / mounted (RamFS In-Memory)", accent_pri, card_bg, true);
    }

    /* -------------------------------------------------------------
     * 2. Right Card: Typographic Chronometer Card (x=720, y=48, w=280, h=136)
     * ------------------------------------------------------------- */
    int r_x = 720;
    int r_y = 48;
    int r_w = 280;
    int r_h = 136;

    if (!(r_x + r_w <= clip_x || r_x >= clip_r || r_y + r_h <= clip_y || r_y >= clip_b)) {
        /* Card Background & Frame */
        gfx_fill_rect(r_x, r_y, r_w, r_h, card_bg);
        gfx_draw_rect(r_x, r_y, r_w, r_h, card_border);

        /* Accent top stripe */
        gfx_fill_rect(r_x + 1, r_y + 1, r_w - 2, 3, card_border);

        /* Card Header */
        gfx_draw_string(r_x + 14, r_y + 12, "DESKTOP CHRONOMETER", accent_pri, card_bg, true);
        gfx_draw_line(r_x + 14, r_y + 30, r_x + r_w - 14, r_y + 30, text_mut);

        /* 2x-Scaled Typographic Clock Digits */
        char clock_buf[16];
        timer_format_clock(clock_buf);
        /* Format clock_buf (e.g. "12:34:56") at 2x scale -> 8 chars * 16px = 128px wide */
        int clock_x = r_x + (r_w - (8 * 16)) / 2;
        gfx_draw_string_scaled(clock_x, r_y + 42, clock_buf, 2, text_pri, card_bg, true);

        /* Date text */
        gfx_draw_string(r_x + 36, r_y + 82, "Monday, October 31, 2026", accent_sec, card_bg, true);

        /* Uptime counter */
        uint32_t up_s = timer_get_uptime_sec();
        char up_buf[32] = "Uptime: ";
        char s_buf[16];
        hud_fmt_uint(up_s, s_buf);
        strlcat(up_buf, s_buf, sizeof(up_buf));
        strlcat(up_buf, " seconds", sizeof(up_buf));
        gfx_draw_string(r_x + 46, r_y + 104, up_buf, text_mut, card_bg, true);
    }
}
