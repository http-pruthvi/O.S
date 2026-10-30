#include "gfx/paint_demo.h"
#include "gfx/gfx.h"
#include "gfx/cursor.h"
#include "gfx/fb_console.h"
#include "drivers/mouse.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"

#define CANVAS_BG           0x00FFFFFF
#define TITLE_BAR_BG        0x001B2A4A
#define PALETTE_BAR_BG      0x00283750
#define FOOTER_BAR_BG       0x001B2A4A
#define BUTTON_BG           0x003A4D6B
#define BUTTON_EXIT_BG      0x00A02020

#define PALETTE_COUNT 10

static const uint32_t palette_colors[PALETTE_COUNT] = {
    GFX_COLOR_BLACK,
    GFX_COLOR_WHITE,
    GFX_COLOR_RED,
    GFX_COLOR_GREEN,
    GFX_COLOR_BLUE,
    GFX_COLOR_YELLOW,
    GFX_COLOR_CYAN,
    GFX_COLOR_MAGENTA,
    GFX_COLOR_ORANGE,
    GFX_COLOR_DARK_GREY
};

static const char* const palette_names[PALETTE_COUNT] = {
    "Black", "White", "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "Orange", "Dark Grey"
};

/*
 * draw_header_and_buttons:
 * Draws the top title bar and action buttons (Clear, Exit) into the back buffer.
 */
static void draw_header_and_buttons(void) {
    /* Title Bar Background (0 to 45) */
    gfx_fill_rect(0, 0, 1024, 46, TITLE_BAR_BG);
    gfx_draw_rect(0, 0, 1024, 46, 0x00405070);

    /* Title text */
    gfx_draw_string(16, 8, "MyOS Paint Demo - Milestone 4 GUI Foundation",
                    GFX_COLOR_WHITE, TITLE_BAR_BG, true);
    gfx_draw_string(16, 26, "L-Click: Draw | R-Click: Erase | Click Palette: Pick Color | Press [ESC] to Exit",
                    GFX_COLOR_LIGHT_GREY, TITLE_BAR_BG, true);

    /* Clear Button: x = 860, y = 8, w = 70, h = 30 */
    gfx_fill_rect(860, 8, 70, 30, BUTTON_BG);
    gfx_draw_rect(860, 8, 70, 30, GFX_COLOR_WHITE);
    gfx_draw_string(875, 15, "CLEAR", GFX_COLOR_WHITE, BUTTON_BG, true);

    /* Exit Button: x = 942, y = 8, w = 68, h = 30 */
    gfx_fill_rect(942, 8, 68, 30, BUTTON_EXIT_BG);
    gfx_draw_rect(942, 8, 68, 30, GFX_COLOR_WHITE);
    gfx_draw_string(958, 15, "EXIT", GFX_COLOR_WHITE, BUTTON_EXIT_BG, true);
}

/*
 * draw_palette_bar:
 * Draws the color swatch selector with the active color highlighted.
 */
static void draw_palette_bar(uint32_t active_color) {
    /* Palette Bar Background (46 to 75) */
    gfx_fill_rect(0, 46, 1024, 30, PALETTE_BAR_BG);
    gfx_draw_line(0, 75, 1023, 75, 0x00405070);

    gfx_draw_string(16, 53, "Palette:", GFX_COLOR_WHITE, PALETTE_BAR_BG, true);

    /* Draw color swatches starting at x = 90 */
    int start_x = 90;
    int swatch_w = 34;
    int swatch_h = 20;
    int spacing = 8;

    for (int i = 0; i < PALETTE_COUNT; i++) {
        int x = start_x + i * (swatch_w + spacing);
        int y = 51;

        /* Fill swatch */
        gfx_fill_rect(x, y, swatch_w, swatch_h, palette_colors[i]);

        /* Highlight selected color with double border */
        if (palette_colors[i] == active_color) {
            gfx_draw_rect(x - 2, y - 2, swatch_w + 4, swatch_h + 4, GFX_COLOR_YELLOW);
            gfx_draw_rect(x - 1, y - 1, swatch_w + 2, swatch_h + 2, GFX_COLOR_WHITE);
        } else {
            gfx_draw_rect(x, y, swatch_w, swatch_h, 0x00506080);
        }
    }

    /* Brush indicator */
    gfx_draw_string(540, 53, "Brush Size: 4 px", GFX_COLOR_LIGHT_GREY, PALETTE_BAR_BG, true);
}

/*
 * draw_footer_status:
 * Formats and draws the status bar at the bottom of the screen (y = 736 to 767).
 */
static void draw_footer_status(int mx, int my, uint8_t buttons, uint32_t active_color) {
    gfx_fill_rect(0, 736, 1024, 32, FOOTER_BAR_BG);
    gfx_draw_line(0, 736, 1023, 736, 0x00405070);

    char buf[128];
    const char* color_name = "Custom";
    for (int i = 0; i < PALETTE_COUNT; i++) {
        if (palette_colors[i] == active_color) {
            color_name = palette_names[i];
            break;
        }
    }

    /* Manually format status line without heavy printf */
    int idx = 0;
    const char p1[] = "Mouse: (";
    for (int i = 0; p1[i]; i++) buf[idx++] = p1[i];

    /* Format X */
    if (mx < 10) buf[idx++] = ' ';
    if (mx < 100) buf[idx++] = ' ';
    if (mx >= 1000) { buf[idx++] = (char)('0' + (mx / 1000)); mx %= 1000; }
    if (mx >= 100)  { buf[idx++] = (char)('0' + (mx / 100));  mx %= 100;  }
    if (mx >= 10)   { buf[idx++] = (char)('0' + (mx / 10));   mx %= 10;   }
    buf[idx++] = (char)('0' + mx);

    const char p2[] = ", ";
    for (int i = 0; p2[i]; i++) buf[idx++] = p2[i];

    /* Format Y */
    if (my < 10) buf[idx++] = ' ';
    if (my < 100) buf[idx++] = ' ';
    if (my >= 100) { buf[idx++] = (char)('0' + (my / 100)); my %= 100; }
    if (my >= 10)  { buf[idx++] = (char)('0' + (my / 10));  my %= 10;  }
    buf[idx++] = (char)('0' + my);

    const char p3[] = ") | Buttons: [L: ";
    for (int i = 0; p3[i]; i++) buf[idx++] = p3[i];
    buf[idx++] = (buttons & MOUSE_BTN_LEFT) ? 'D' : 'U';
    buf[idx++] = (buttons & MOUSE_BTN_LEFT) ? 'N' : 'P';

    const char p4[] = ", R: ";
    for (int i = 0; p4[i]; i++) buf[idx++] = p4[i];
    buf[idx++] = (buttons & MOUSE_BTN_RIGHT) ? 'D' : 'U';
    buf[idx++] = (buttons & MOUSE_BTN_RIGHT) ? 'N' : 'P';

    const char p5[] = "] | Active Color: ";
    for (int i = 0; p5[i]; i++) buf[idx++] = p5[i];
    for (int i = 0; color_name[i]; i++) buf[idx++] = color_name[i];

    buf[idx] = '\0';

    gfx_draw_string(16, 744, buf, GFX_COLOR_WHITE, FOOTER_BAR_BG, true);

    /* Draw preview swatch in status bar */
    gfx_fill_rect(980, 742, 24, 18, active_color);
    gfx_draw_rect(980, 742, 24, 18, GFX_COLOR_WHITE);
}

/*
 * paint_demo_run:
 * Main interactive loop for the Mouse Paint Demo.
 */
void paint_demo_run(void) {
    serial_writestring("[PAINT] Starting Mouse Paint Demo...\n");

    /* 1. Ensure 2D graphics back buffer is initialized */
    if (!gfx_init()) {
        serial_writestring("[PAINT] ERROR: Back buffer allocation failed! Aborting paint demo.\n");
        kprintf("[PAINT] ERROR: Back buffer allocation failed! Returning to console.\n");
        return;
    }

    /* 2. Initialize Canvas (y = 76 to 735) and UI chrome in back buffer */
    gfx_fill_rect(0, 76, 1024, 660, CANVAS_BG);
    draw_header_and_buttons();
    uint32_t active_color = GFX_COLOR_BLACK;
    draw_palette_bar(active_color);

    int cur_mx = 512, cur_my = 384;
    mouse_get_position(&cur_mx, &cur_my);
    draw_footer_status(cur_mx, cur_my, 0, active_color);

    /* 3. Flush full scene to VRAM (double buffering) */
    gfx_flush();

    /* 4. Initialize and display software cursor */
    cursor_init();
    cursor_move(cur_mx, cur_my);
    cursor_show();

    serial_writestring("[PAINT] Paint demo active. Move mouse to draw, ESC to exit.\n");

    int prev_x = -1;
    int prev_y = -1;
    bool is_drawing = false;
    int brush_size = 4;
    int last_status_x = -1, last_status_y = -1;
    uint8_t last_status_btn = 0xFF;

    /* 5. Interactive Event Loop */
    while (1) {
        /* Check keyboard for exit command (ESC key) */
        if (keyboard_has_char()) {
            char c = keyboard_getchar();
            if (c == 27) { /* ESC key */
                serial_writestring("[PAINT] ESC key detected. Exiting paint demo...\n");
                break;
            }
        }

        /* Process pending mouse events */
        mouse_event_t ev;
        while (mouse_poll_event(&ev)) {
            int mx, my;
            mouse_get_position(&mx, &my);

            /* Move cursor (touches ONLY the two 12x19 rectangles) */
            cursor_move(mx, my);

            /* Check button clicks */
            if (ev.buttons & MOUSE_BTN_LEFT) {
                /* Check Clear button: x in [860, 930], y in [8, 38] */
                if (mx >= 860 && mx <= 930 && my >= 8 && my <= 38) {
                    gfx_fill_rect(0, 76, 1024, 660, CANVAS_BG);
                    gfx_flush_rect(0, 76, 1024, 660);
                    is_drawing = false;
                    prev_x = -1;
                    prev_y = -1;
                    continue;
                }

                /* Check Exit button: x in [942, 1010], y in [8, 38] */
                if (mx >= 942 && mx <= 1010 && my >= 8 && my <= 38) {
                    serial_writestring("[PAINT] Exit button clicked. Exiting paint demo...\n");
                    goto exit_paint;
                }

                /* Check Palette selection: y in [46, 75], x in [90, 510] */
                if (my >= 46 && my <= 75 && mx >= 90 && mx < 90 + PALETTE_COUNT * 42) {
                    int swatch_idx = (mx - 90) / 42;
                    if (swatch_idx >= 0 && swatch_idx < PALETTE_COUNT) {
                        active_color = palette_colors[swatch_idx];
                        draw_palette_bar(active_color);
                        gfx_flush_rect(0, 46, 1024, 30);
                        draw_footer_status(mx, my, ev.buttons, active_color);
                        gfx_flush_rect(0, 736, 1024, 32);
                        is_drawing = false;
                        prev_x = -1;
                        prev_y = -1;
                        continue;
                    }
                }
            }

            /* Painting in Canvas Area (y >= 76 and y < 736) */
            bool left_down  = (ev.buttons & MOUSE_BTN_LEFT) != 0;
            bool right_down = (ev.buttons & MOUSE_BTN_RIGHT) != 0;

            if (left_down || right_down) {
                uint32_t draw_color = left_down ? active_color : CANVAS_BG;

                if (my >= 76 && my < 736) {
                    if (is_drawing && prev_x >= 0 && prev_y >= 76 && prev_y < 736) {
                        /* Continuous stroke: draw thick line along Bresenham path */
                        gfx_draw_line_thick(prev_x, prev_y, mx, my, brush_size, draw_color);

                        /* Calculate bounding box of the stroke */
                        int min_x = (prev_x < mx ? prev_x : mx) - brush_size;
                        int min_y = (prev_y < my ? prev_y : my) - brush_size;
                        int max_x = (prev_x > mx ? prev_x : mx) + brush_size;
                        int max_y = (prev_y > my ? prev_y : my) + brush_size;

                        /* Clamp dirty rect to canvas boundaries */
                        if (min_x < 0) min_x = 0;
                        if (min_y < 76) min_y = 76;
                        if (max_x >= 1024) max_x = 1023;
                        if (max_y >= 736) max_y = 735;

                        /* Flush ONLY the dirty bounding box */
                        gfx_flush_rect(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1);
                    } else {
                        /* Single point / stamp */
                        int half = brush_size / 2;
                        gfx_fill_rect(mx - half, my - half, brush_size, brush_size, draw_color);
                        gfx_flush_rect(mx - half, my - half, brush_size, brush_size);
                    }

                    prev_x = mx;
                    prev_y = my;
                    is_drawing = true;
                } else {
                    is_drawing = false;
                    prev_x = -1;
                    prev_y = -1;
                }
            } else {
                is_drawing = false;
                prev_x = -1;
                prev_y = -1;
            }

            /* Update footer status bar if mouse coordinates or buttons changed */
            if (mx != last_status_x || my != last_status_y || ev.buttons != last_status_btn) {
                last_status_x = mx;
                last_status_y = my;
                last_status_btn = ev.buttons;
                draw_footer_status(mx, my, ev.buttons, active_color);
                gfx_flush_rect(0, 736, 1024, 32);
            }
        }

        /* Sleep CPU until next hardware interrupt (timer or mouse or keyboard) */
        __asm__ volatile ("hlt");
    }

exit_paint:
    /* 6. Clean exit back to framebuffer console */
    cursor_hide();
    fb_console_repaint();

    /* Redraw top-right uptime clock */
    if (fb_is_active()) {
        char clock_buf[16];
        timer_format_clock(clock_buf);
        vga_put_at(112, 0, clock_buf, 0x1F);
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n[PAINT] Returned from Paint Demo to Interactive Console.\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    serial_writestring("[PAINT] Paint demo exited. Framebuffer console restored.\n");
}
