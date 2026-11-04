#include "gui/desktop.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gui/hud.h"
#include "gui/waybar.h"
#include "gui/dock.h"
#include "gui/lockscreen.h"
#include "apps/launcher.h"
#include "apps/clock.h"
#include "apps/explorer.h"
#include "apps/taskmgr.h"
#include "apps/terminal.h"
#include "apps/control.h"
#include "apps/settings.h"
#include "apps/minesweeper.h"
#include "apps/snake.h"
#include "apps/photos.h"
#include "apps/player.h"
#include "apps/breakout.h"
#include "apps/friday.h"
#include "apps/store.h"
#include "gui/startmenu.h"
#include "gui/notification.h"
#include "gfx/gfx.h"
#include "gfx/cursor.h"
#include "gfx/fb_console.h"
#include "drivers/mouse.h"
#include "drivers/keyboard.h"
#include "drivers/timer.h"
#include "drivers/vga.h"
#include "drivers/serial.h"
#include "mm/pmm.h"
#include "mm/heap.h"
#include "proc/scheduler.h"
#include "lib/kprintf.h"

extern void spawn_compute_task(void);

/* State for Window 1 (Controls) */
static int       demo_counter = 0;
static widget_t* lbl_counter  = NULL;
static widget_t* lbl_echo     = NULL;

/* State for Window 2 (System Monitor) */
static widget_t* lbl_pmm_free  = NULL;
static widget_t* lbl_heap_used = NULL;
static widget_t* lbl_tasks     = NULL;

/* Reference to Window 3 */
static window_t* win_about = NULL;
static bool s_desktop_running = false;

bool desktop_is_running(void) {
    return s_desktop_running;
}

static void on_about_win_close(window_t* win) {
    (void)win;
    if (win_about) {
        wm_remove_window(win_about);
        win_about = NULL;
    }
}

/* Helper to convert integer to decimal string */
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

/* Callback: Increment Counter */
static void on_btn_inc(widget_t* w, void* data) {
    (void)w; (void)data;
    demo_counter++;
    char buf[32] = "Counter Value: ";
    int_to_str(demo_counter, buf + 15);
    widget_set_text(lbl_counter, buf);
}

/* Callback: Decrement Counter */
static void on_btn_dec(widget_t* w, void* data) {
    (void)w; (void)data;
    demo_counter--;
    char buf[32] = "Counter Value: ";
    int_to_str(demo_counter, buf + 15);
    widget_set_text(lbl_counter, buf);
}

/* Callback: Reset Counter */
static void on_btn_reset(widget_t* w, void* data) {
    (void)w; (void)data;
    demo_counter = 0;
    widget_set_text(lbl_counter, "Counter Value: 0");
}

/* Callback: Live Textbox Typing Echo */
static void on_textbox_changed(widget_t* w, const char* text) {
    (void)w;
    char buf[64] = "Echo: ";
    int idx = 6;
    while (*text && idx < 60) {
        buf[idx++] = *text++;
    }
    buf[idx] = '\0';
    widget_set_text(lbl_echo, buf);
}

/* Callback: Refresh System Stats */
static void on_btn_refresh_stats(widget_t* w, void* data) {
    (void)w; (void)data;

    /* PMM Stats */
    uint32_t free_f = pmm_free_frames();
    char pmm_buf[48] = "PMM Free Frames: ";
    int_to_str((int)free_f, pmm_buf + 17);
    widget_set_text(lbl_pmm_free, pmm_buf);

    /* Heap Stats */
    size_t used_b = 0, free_b = 0, blocks = 0;
    heap_stats(&used_b, &free_b, &blocks);
    char heap_buf[48] = "Heap Used: ";
    int_to_str((int)(used_b / 1024), heap_buf + 11);
    int len = 0; while (heap_buf[len]) len++;
    heap_buf[len++] = ' '; heap_buf[len++] = 'K'; heap_buf[len++] = 'B';
    heap_buf[len] = '\0';
    widget_set_text(lbl_heap_used, heap_buf);

    /* Tasks Stats */
    uint32_t tasks = task_get_count();
    char task_buf[48] = "Active Tasks: ";
    int_to_str((int)tasks, task_buf + 14);
    int tlen = 0; while (task_buf[tlen]) tlen++;
    task_buf[tlen++] = ' '; task_buf[tlen++] = '('; task_buf[tlen++] = 'P'; task_buf[tlen++] = 'r';
    task_buf[tlen++] = 'e'; task_buf[tlen++] = 'e'; task_buf[tlen++] = 'm'; task_buf[tlen++] = 'p';
    task_buf[tlen++] = 't'; task_buf[tlen++] = 'i'; task_buf[tlen++] = 'v'; task_buf[tlen++] = 'e';
    task_buf[tlen++] = ')'; task_buf[tlen] = '\0';
    if (lbl_tasks) {
        widget_set_text(lbl_tasks, task_buf);
    }
}

/* Callback: Close About Window */
static void on_btn_about_close(widget_t* w, void* data) {
    (void)w; (void)data;
    if (win_about) {
        wm_remove_window(win_about);
        win_about = NULL;
    }
}

static void setup_showcase_windows(void) {
    /* -------------------------------------------------------------
     * Window 1: Widget Controls Showcase (Left side)
     * ------------------------------------------------------------- */
    window_t* win1 = window_create(60, 60, 420, 360, "Widget Showcase",
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (win1) {
        /* Title and description labels */
        widget_t* lbl_title = widget_create_label(20, 16, 380, 20, "Interactive GUI Controls", 0x0060A5FA);
        widget_add_to_window(win1, lbl_title);

        /* Counter Section */
        lbl_counter = widget_create_label(20, 45, 380, 20, "Counter Value: 0", 0x00FFFFFF);
        widget_add_to_window(win1, lbl_counter);

        widget_t* btn_dec = widget_create_button(20, 75, 110, 30, "[-] Dec", on_btn_dec, NULL);
        widget_add_to_window(win1, btn_dec);

        widget_t* btn_inc = widget_create_button(140, 75, 110, 30, "[+] Inc", on_btn_inc, NULL);
        widget_add_to_window(win1, btn_inc);

        widget_t* btn_rst = widget_create_button(260, 75, 90, 30, "Reset", on_btn_reset, NULL);
        widget_add_to_window(win1, btn_rst);

        /* Text Input Section */
        widget_t* lbl_prompt = widget_create_label(20, 130, 380, 20, "Type something below:", 0x00E0E0E0);
        widget_add_to_window(win1, lbl_prompt);

        widget_t* txt_input = widget_create_textbox(20, 155, 360, 32, "Click & type...", 40);
        txt_input->on_change = on_textbox_changed;
        widget_add_to_window(win1, txt_input);

        lbl_echo = widget_create_label(20, 205, 380, 20, "Echo: Click & type...", 0x0060A5FA);
        widget_add_to_window(win1, lbl_echo);

        /* Status info */
        widget_t* lbl_hint = widget_create_label(20, 260, 380, 20, "Tip: Drag title bar to move window!", 0x009E9E9E);
        widget_add_to_window(win1, lbl_hint);

        widget_t* lbl_hint2 = widget_create_label(20, 285, 380, 20, "Press [ESC] anytime to exit to console.", 0x009E9E9E);
        widget_add_to_window(win1, lbl_hint2);

        wm_add_window(win1);
    }

    /* -------------------------------------------------------------
     * Window 2: System Diagnostics (Right side)
     * ------------------------------------------------------------- */
    window_t* win2 = window_create(540, 100, 420, 340, "System Monitor",
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (win2) {
        widget_t* lbl_diag = widget_create_label(20, 16, 380, 20, "Hardware & Memory Status", 0x0060A5FA);
        widget_add_to_window(win2, lbl_diag);

        widget_t* lbl_ram = widget_create_label(20, 48, 380, 20, "Total RAM: 128 MB (Identity Paged)", 0x00FFFFFF);
        widget_add_to_window(win2, lbl_ram);

        uint32_t free_f = pmm_free_frames();
        char pmm_buf[48] = "PMM Free Frames: ";
        int_to_str((int)free_f, pmm_buf + 17);
        lbl_pmm_free = widget_create_label(20, 76, 380, 20, pmm_buf, 0x00FFFFFF);
        widget_add_to_window(win2, lbl_pmm_free);

        widget_t* lbl_heap_base = widget_create_label(20, 104, 380, 20, "Heap Virtual Base: 0xD0000000", 0x00FFFFFF);
        widget_add_to_window(win2, lbl_heap_base);

        size_t used_b = 0, free_b = 0, blocks = 0;
        heap_stats(&used_b, &free_b, &blocks);
        char heap_buf[48] = "Heap Used: ";
        int_to_str((int)(used_b / 1024), heap_buf + 11);
        int len = 0; while (heap_buf[len]) len++;
        heap_buf[len++] = ' '; heap_buf[len++] = 'K'; heap_buf[len++] = 'B';
        heap_buf[len] = '\0';
        lbl_heap_used = widget_create_label(20, 132, 380, 20, heap_buf, 0x00FFFFFF);
        widget_add_to_window(win2, lbl_heap_used);

        widget_t* lbl_res = widget_create_label(20, 160, 380, 20, "Display: 1024x768x32 Linear RGB", 0x00FFFFFF);
        widget_add_to_window(win2, lbl_res);

        uint32_t tasks = task_get_count();
        char task_buf[48] = "Active Tasks: ";
        int_to_str((int)tasks, task_buf + 14);
        int tlen = 0; while (task_buf[tlen]) tlen++;
        task_buf[tlen++] = ' '; task_buf[tlen++] = '('; task_buf[tlen++] = 'P'; task_buf[tlen++] = 'r';
        task_buf[tlen++] = 'e'; task_buf[tlen++] = 'e'; task_buf[tlen++] = 'm'; task_buf[tlen++] = 'p';
        task_buf[tlen++] = 't'; task_buf[tlen++] = 'i'; task_buf[tlen++] = 'v'; task_buf[tlen++] = 'e';
        task_buf[tlen++] = ')'; task_buf[tlen] = '\0';
        lbl_tasks = widget_create_label(20, 188, 380, 20, task_buf, 0x00FFFFFF);
        widget_add_to_window(win2, lbl_tasks);

        widget_t* btn_refresh = widget_create_button(20, 230, 180, 32, "Refresh Metrics", on_btn_refresh_stats, NULL);
        widget_add_to_window(win2, btn_refresh);

        wm_add_window(win2);
    }

    /* -------------------------------------------------------------
     * Window 3: About MyOS (Center Overlapping Window)
     * ------------------------------------------------------------- */
    win_about = window_create(280, 180, 440, 270, "About MyOS",
                              WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (win_about) {
        widget_t* lbl_app = widget_create_label(20, 16, 400, 20, "MyOS Desktop Environment v1.0", 0x0060A5FA);
        widget_add_to_window(win_about, lbl_app);

        widget_t* lbl_sub = widget_create_label(20, 42, 400, 20, "B.Tech Final Year Operating System Project", 0x00E0E0E0);
        widget_add_to_window(win_about, lbl_sub);

        widget_t* lbl_d1 = widget_create_label(20, 80, 400, 20, "- Multi-Window Z-Order Compositor", 0x00FFFFFF);
        widget_add_to_window(win_about, lbl_d1);

        widget_t* lbl_d2 = widget_create_label(20, 105, 400, 20, "- Reusable Widgets: Buttons, Labels, TextBoxes", 0x00FFFFFF);
        widget_add_to_window(win_about, lbl_d2);

        widget_t* lbl_d3 = widget_create_label(20, 130, 400, 20, "- Tear-Free Software Cursor & Save-Under", 0x00FFFFFF);
        widget_add_to_window(win_about, lbl_d3);

        widget_t* lbl_d4 = widget_create_label(20, 155, 400, 20, "- Taskbar with Windows Switching & Clock", 0x00FFFFFF);
        widget_add_to_window(win_about, lbl_d4);

        widget_t* btn_ok = widget_create_button(160, 195, 120, 32, "Dismiss", on_btn_about_close, NULL);
        widget_add_to_window(win_about, btn_ok);
        win_about->on_close = on_about_win_close;

        wm_add_window(win_about);
    }
}

/*
 * app_showcase_open:
 * Opens the 3 demonstration windows with widgets on demand.
 */
void app_showcase_open(void) {
    setup_showcase_windows();
    wm_redraw_all();
}

/*
 * desktop_run:
 * Enters the interactive desktop environment.
 */
void desktop_run(void) {
    serial_writestring("[DESKTOP] Launching Desktop Window Manager environment...\n");

    s_desktop_running = true;

    /* 1. Initialize Window Manager and allocate back buffer */
    if (!wm_init()) {
        serial_writestring("[DESKTOP] ERROR: Window manager initialization failed!\n");
        kprintf("[DESKTOP] ERROR: wm_init() failed! Returning to console.\n");
        return;
    }

    /* Initialize Application Launcher */
    launcher_init();

    /* Pre-seed sample PPM images into RamFS */
    photos_preseed_images();

    /* Initialize Friday AI Assistant Subsystem */
    friday_init();

    /* Push Welcome Toast Notification */
    notification_push(NOTIFY_SUCCESS, "Windows 11 Fluent OS", "System ready. Clean desktop active!");

    /* 2. Clean desktop startup: apps are pinned on Start Bar and accessible via Search */
    serial_writestring("[DESKTOP] Clean desktop initialized with zero open windows.\n");

    /* 3. Render desktop background, taskbar, and windows */
    wm_redraw_all();

    /* 4. Display software mouse cursor */
    cursor_init();
    int mx = 512, my = 384;
    mouse_get_position(&mx, &my);
    cursor_move(mx, my);
    cursor_show();

    serial_writestring("[DESKTOP] Desktop active. Drag windows, click widgets, ESC to exit.\n");

    /* 5. Desktop Event Loop */
    while (1) {
        /* Check if shutdown requested via Start Menu */
        if (launcher_should_exit()) {
            serial_writestring("[DESKTOP] Exit requested via Start Menu. Returning to console...\n");
            break;
        }

        /* A. Process Keyboard Input */
        if (keyboard_has_char()) {
            char c = keyboard_getchar();
            if (c == 27) { /* ESC key exits desktop */
                serial_writestring("[DESKTOP] ESC pressed. Exiting desktop to console...\n");
                break;
            }

            /* Intercept all keys if lock screen is active */
            if (lockscreen_is_active()) {
                lockscreen_handle_key(c);
                continue;
            }

            /* Intercept all keys if Start Menu / Search is open */
            if (startmenu_is_open()) {
                if (startmenu_handle_key(c)) {
                    continue;
                }
            }

            if ((unsigned char)c == KEY_F1) {
                serial_writestring("[DESKTOP] F1 pressed: Toggling Start Menu...\n");
                startmenu_toggle();
                wm_redraw_all();
                continue;
            }
            if ((unsigned char)c == KEY_F2) {
                serial_writestring("[DESKTOP] F2 pressed: Opening Photos Viewer...\n");
                app_photos_open();
                continue;
            }
            if ((unsigned char)c == KEY_F3) {
                serial_writestring("[DESKTOP] F3 pressed: Opening Media Player...\n");
                app_player_open();
                continue;
            }
            if ((unsigned char)c == KEY_F4) {
                serial_writestring("[DESKTOP] F4 pressed: Opening Breakout Game...\n");
                app_breakout_open();
                continue;
            }
            if ((unsigned char)c == KEY_F5) {
                serial_writestring("[DESKTOP] F5 pressed: Toggling Conky HUD widgets...\n");
                hud_toggle();
                wm_redraw_all();
                continue;
            }
            if ((unsigned char)c == KEY_F6) {
                serial_writestring("[DESKTOP] F6 pressed: Locking session...\n");
                lockscreen_activate();
                continue;
            }
            if ((unsigned char)c == KEY_F7) {
                serial_writestring("[DESKTOP] F7 pressed: Opening System Settings...\n");
                app_settings_open();
                continue;
            }
            if ((unsigned char)c == KEY_F8) {
                serial_writestring("[DESKTOP] F8 pressed: Opening File Explorer...\n");
                app_explorer_open();
                continue;
            }
            if ((unsigned char)c == KEY_F9) {
                serial_writestring("[DESKTOP] F9 pressed: Opening Terminal...\n");
                app_terminal_open();
                continue;
            }
            if ((unsigned char)c == KEY_F10) {
                serial_writestring("[DESKTOP] F10 pressed: Opening Minesweeper...\n");
                app_minesweeper_open();
                continue;
            }
            if ((unsigned char)c == KEY_F11) {
                serial_writestring("[DESKTOP] F11 pressed: Spawning background worker task...\n");
                spawn_compute_task();
                continue;
            }
            if ((unsigned char)c == KEY_F12) {
                serial_writestring("[DESKTOP] F12 pressed: Opening Task Manager...\n");
                app_taskmgr_open();
                continue;
            }

            /* Forward raw keystrokes to Breakout if running */
            breakout_handle_key((uint8_t)c);

            /* Dispatch keystroke to focused window / text box */
            wm_handle_keyboard(c);
        }

        /* B. Process Mouse Events */
        mouse_event_t ev;
        while (mouse_poll_event(&ev)) {
            mouse_get_position(&mx, &my);

            /* Move cursor smoothly */
            cursor_move(mx, my);

            /* Dispatch mouse event to window manager (dragging, focus, clicks) */
            wm_handle_mouse(&ev, mx, my);
        }

        /* C. Update Periodic Timers & Ticks */
        static uint32_t s_last_desktop_tick = 0;
        uint32_t cur_tick = timer_get_ticks();
        if (cur_tick != s_last_desktop_tick) {
            uint32_t delta = cur_tick - s_last_desktop_tick;
            s_last_desktop_tick = cur_tick;
            if (delta > 5) delta = 5;
            for (uint32_t i = 0; i < delta; i++) {
                notification_tick();
                player_tick();
                breakout_tick();
                friday_tick();
            }

            /* Periodically animate Friday AI Orb (every ~3 ticks) */
            static uint32_t s_orb_anim_counter = 0;
            s_orb_anim_counter += delta;
            if (s_orb_anim_counter >= 3) {
                s_orb_anim_counter = 0;
                wm_invalidate_rect(870, 540, 120, 130);
            }
        }

        /* D. Update Taskbar Clock & Clock App */
        if (timer_is_clock_dirty()) {
            wm_render_taskbar(WM_SCREEN_WIDTH - 120, WM_TASKBAR_Y, 120, WM_TASKBAR_HEIGHT);
            gfx_flush_rect(WM_SCREEN_WIDTH - 120, WM_TASKBAR_Y, 120, WM_TASKBAR_HEIGHT);
            app_clock_update();
            timer_clear_clock_dirty();
        }

        /* Sleep CPU until next hardware interrupt */
        __asm__ volatile ("hlt");
    }

    /* 6. Clean Exit back to Framebuffer Console */
    cursor_hide();
    fb_console_repaint();

    /* Redraw top-right uptime clock in console */
    if (fb_is_active()) {
        char clock_buf[16];
        timer_format_clock(clock_buf);
        vga_put_at(112, 0, clock_buf, 0x1F);
    }

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    kprintf("\n[DESKTOP] Returned from Desktop Environment to Interactive Console.\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    serial_writestring("[DESKTOP] Desktop exited. Framebuffer console restored.\n");
}

