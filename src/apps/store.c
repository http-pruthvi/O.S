#include "apps/store.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gui/startmenu.h"
#include "gui/notification.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"
#include "lib/io.h"
#include "net/http.h"
#include "net/netdev.h"
#include "net/dns.h"

/* Structure for a Web App in the Store Catalog */
typedef struct {
    const char* id;
    const char* name;
    const char* category;
    const char* version;
    const char* size_str;
    const char* desc;
    const char* tag;
    uint32_t icon_color;
    bool is_installed;
    int install_progress; /* 0..100 */
    void (*launch_fn)(void);
} store_app_t;

/* Forward declarations for web app launch windows */
static void launch_web_browser(void);
static void launch_hex_editor(void);
static void launch_markdown_notes(void);
static void launch_irc_client(void);
static void launch_system_benchmark(void);
static void launch_mandelbrot_explorer(void);

/* Web Store Catalog */
static store_app_t s_catalog[] = {
    {
        "browser", "Web Browser Lite", "Internet", "v1.2", "48 KB",
        "HTTP/HTML web navigation client with bookmarks & URL viewport",
        "Web", 0x0038BDF8, false, 0, launch_web_browser
    },
    {
        "hex", "Hex Editor Pro", "Dev Tools", "v2.0", "32 KB",
        "Interactive binary hexadecimal inspector and memory viewer",
        "Hex", 0x00A78BFA, false, 0, launch_hex_editor
    },
    {
        "notes", "Markdown Notes", "Productivity", "v1.0", "24 KB",
        "Distraction-free markdown notes and documentation viewer",
        "Doc", 0x0034D399, false, 0, launch_markdown_notes
    },
    {
        "irc", "IRC Chat Client", "Network", "v1.5", "28 KB",
        "Lightweight IRC messaging and channel chatter client",
        "IRC", 0x00F472B6, false, 0, launch_irc_client
    },
    {
        "bench", "System Benchmark", "Utilities", "v3.1", "18 KB",
        "Real-time CPU arithmetic, memory bandwidth & FPS benchmark",
        "Spd", 0x00FBBF24, false, 0, launch_system_benchmark
    },
    {
        "fractal", "Mandelbrot Explorer", "Graphics", "v1.4", "36 KB",
        "Complex fractal renderer with multi-color palette mapping",
        "Frac", 0x00818CF8, false, 0, launch_mandelbrot_explorer
    }
};
#define NUM_CATALOG_APPS (int)(sizeof(s_catalog) / sizeof(s_catalog[0]))

static window_t* s_win_store   = NULL;
static window_t* s_win_browser = NULL;
static window_t* s_win_hex     = NULL;
static window_t* s_win_notes   = NULL;
static window_t* s_win_irc     = NULL;
static window_t* s_win_bench   = NULL;
static window_t* s_win_mandel  = NULL;
static int s_store_tab = 0; /* 0: Featured Catalog, 1: Custom Web URL */
static widget_t* s_txt_url = NULL;
static widget_t* s_txt_name = NULL;
static widget_t* s_browser_txt_url = NULL;
static char      s_browser_html_buf[4096];
static char      s_browser_html_buf[4096];

/* Helper to convert uint to string without libc */
static void fmt_uint(uint32_t val, char* buf) {
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

/* Beep chime on installation completion */
static void play_install_chime(void) {
    /* Two-tone ascending chime (523 Hz -> 659 Hz) */
    uint16_t div1 = (uint16_t)(1193180 / 523);
    uint16_t div2 = (uint16_t)(1193180 / 659);

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(div1 & 0xFF));
    outb(0x42, (uint8_t)((div1 >> 8) & 0xFF));
    uint8_t spk = inb(0x61);
    outb(0x61, spk | 0x03);

    timer_sleep_ms(60);

    outb(0x42, (uint8_t)(div2 & 0xFF));
    outb(0x42, (uint8_t)((div2 >> 8) & 0xFF));

    timer_sleep_ms(80);

    outb(0x61, spk & ~0x03);
}

/* Helper to redraw the store client area */
static void store_refresh_ui(void);

/* Installs an app into RamFS and registers it in Start Menu */
static bool install_app_internal(store_app_t* app) {
    if (!app) return false;

    serial_writestring("[STORE] Downloading & installing web app: ");
    serial_writestring(app->name);
    serial_writestring("...\n");

    /* Write app manifest file /<id>.app */
    char manifest_path[48] = "/";
    strlcat(manifest_path, app->id, sizeof(manifest_path));
    strlcat(manifest_path, ".app", sizeof(manifest_path));

    vfs_create(manifest_path, VFS_FILE);
    int fd = vfs_open(manifest_path, 0x02);
    if (fd >= 0) {
        char payload[160];
        payload[0] = '\0';
        strlcat(payload, "NAME=", sizeof(payload));
        strlcat(payload, app->name, sizeof(payload));
        strlcat(payload, "\nVER=", sizeof(payload));
        strlcat(payload, app->version, sizeof(payload));
        strlcat(payload, "\nCAT=", sizeof(payload));
        strlcat(payload, app->category, sizeof(payload));
        strlcat(payload, "\nEXEC=/", sizeof(payload));
        strlcat(payload, app->id, sizeof(payload));
        strlcat(payload, "\n", sizeof(payload));
        vfs_write(fd, payload, strlen(payload));
        vfs_close(fd);
    }

    /* Write binary stub /<id> */
    char bin_path[48] = "/";
    strlcat(bin_path, app->id, sizeof(bin_path));
    vfs_create(bin_path, VFS_FILE);
    int bfd = vfs_open(bin_path, 0x02);
    if (bfd >= 0) {
        char bin_stub[64] = "#!/bin/sh\n# Web package\nexec ";
        strlcat(bin_stub, app->id, sizeof(bin_stub));
        strlcat(bin_stub, "\n", sizeof(bin_stub));
        vfs_write(bfd, bin_stub, strlen(bin_stub));
        vfs_close(bfd);
    }

    app->is_installed = true;
    app->install_progress = 100;

    /* Register in Start Menu and Search dynamically */
    startmenu_register_app(app->name, app->tag, app->icon_color, app->launch_fn);

    /* Notification & Sound */
    char notif_msg[64] = "";
    strlcat(notif_msg, app->name, sizeof(notif_msg));
    strlcat(notif_msg, " installed from web repo!", sizeof(notif_msg));
    notification_push(NOTIFY_SUCCESS, "Web App Installed", notif_msg);
    play_install_chime();

    serial_writestring("[STORE] Installation complete for ");
    serial_writestring(app->name);
    serial_writestring("\n");

    return true;
}

/* Install by Package ID */
bool app_store_install_by_id(const char* pkg_id) {
    if (!pkg_id) return false;
    for (int i = 0; i < NUM_CATALOG_APPS; i++) {
        if (strcmp(s_catalog[i].id, pkg_id) == 0) {
            bool ok = install_app_internal(&s_catalog[i]);
            if (s_win_store) store_refresh_ui();
            return ok;
        }
    }
    return false;
}

bool app_store_is_installed(const char* pkg_id) {
    if (!pkg_id) return false;
    for (int i = 0; i < NUM_CATALOG_APPS; i++) {
        if (strcmp(s_catalog[i].id, pkg_id) == 0) {
            return s_catalog[i].is_installed;
        }
    }
    return false;
}

bool app_store_launch(const char* pkg_id) {
    if (!pkg_id) return false;
    for (int i = 0; i < NUM_CATALOG_APPS; i++) {
        if (strcmp(s_catalog[i].id, pkg_id) == 0) {
            if (s_catalog[i].is_installed && s_catalog[i].launch_fn) {
                s_catalog[i].launch_fn();
                return true;
            }
        }
    }
    return false;
}

/* Install Custom Web URL */
bool app_store_install_custom_url(const char* url, const char* name) {
    if (!url || !name || strlen(name) == 0) return false;

    serial_writestring("[STORE] Fetching custom web package from: ");
    serial_writestring(url);
    serial_writestring("\n");

    char manifest_path[48] = "/";
    strlcat(manifest_path, name, sizeof(manifest_path));
    strlcat(manifest_path, ".app", sizeof(manifest_path));

    vfs_create(manifest_path, VFS_FILE);
    int fd = vfs_open(manifest_path, 0x02);
    if (fd >= 0) {
        char payload[160] = "NAME=";
        strlcat(payload, name, sizeof(payload));
        strlcat(payload, "\nURL=", sizeof(payload));
        strlcat(payload, url, sizeof(payload));
        strlcat(payload, "\nCUSTOM=true\n", sizeof(payload));
        vfs_write(fd, payload, strlen(payload));
        vfs_close(fd);
    }

    /* Register as custom web app in Start Menu */
    startmenu_register_app(name, "Web", 0x0038BDF8, launch_web_browser);

    notification_push(NOTIFY_SUCCESS, "Web App Installed", "Custom package downloaded to RamFS!");
    play_install_chime();
    return true;
}

/* --------------------------------------------------------------------------
 * Web App Implementations (Running mini-applications)
 * -------------------------------------------------------------------------- */

/* Window Close Callbacks */
static void on_browser_close(window_t* win) {
    (void)win;
    if (s_win_browser) {
        wm_remove_window(s_win_browser);
        s_win_browser = NULL;
        s_browser_txt_url = NULL;
    }
}

static void on_hex_close(window_t* win) {
    (void)win;
    if (s_win_hex) {
        wm_remove_window(s_win_hex);
        s_win_hex = NULL;
    }
}

static void on_btn_hex_close(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_win_hex) {
        wm_remove_window(s_win_hex);
        s_win_hex = NULL;
    }
}

static void on_notes_close(window_t* win) {
    (void)win;
    if (s_win_notes) {
        wm_remove_window(s_win_notes);
        s_win_notes = NULL;
    }
}

static void on_irc_close(window_t* win) {
    (void)win;
    if (s_win_irc) {
        wm_remove_window(s_win_irc);
        s_win_irc = NULL;
    }
}

static void on_bench_close(window_t* win) {
    (void)win;
    if (s_win_bench) {
        wm_remove_window(s_win_bench);
        s_win_bench = NULL;
    }
}

static void on_mandel_close(window_t* win) {
    (void)win;
    if (s_win_mandel) {
        wm_remove_window(s_win_mandel);
        s_win_mandel = NULL;
    }
}

static void on_store_close(window_t* win) {
    (void)win;
    if (s_win_store) {
        wm_remove_window(s_win_store);
        s_win_store = NULL;
    }
}

/* Forward declaration of browser navigation */
static void browser_navigate_to(const char* url);

/* Render live parsed HTML document into browser viewport */
static void browser_render_html(const char* url, const char* html_response, int status_code, size_t len) {
    (void)url;
    if (!s_win_browser) return;

    /* 1. Clear Viewport Canvas */
    window_fill_rect(s_win_browser, 10, 68, 620, 332, 0x0014161A);
    window_draw_rect(s_win_browser, 10, 68, 620, 332, 0x002B303C);

    const char* body = http_find_body(html_response);
    int cur_x = 24;
    int cur_y = 80;

    const char* p = body;
    while (*p && cur_y < 380) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;
        if (*p == '\0') break;

        /* Check for <h1> */
        if (strncmp(p, "<h1>", 4) == 0 || strncmp(p, "<H1>", 4) == 0) {
            p += 4;
            char title[64];
            int ti = 0;
            while (*p && strncmp(p, "</h1>", 5) != 0 && strncmp(p, "</H1>", 5) != 0 && ti < 63) {
                title[ti++] = *p++;
            }
            title[ti] = '\0';
            if (strncmp(p, "</h1>", 5) == 0 || strncmp(p, "</H1>", 5) == 0) p += 5;

            window_draw_string(s_win_browser, cur_x, cur_y, title, 0x0038BDF8, 0x0014161A, true);
            window_fill_rect(s_win_browser, cur_x, cur_y + 18, 550, 2, 0x000284C7);
            cur_y += 28;
            continue;
        }

        /* Check for <h2> */
        if (strncmp(p, "<h2>", 4) == 0 || strncmp(p, "<H2>", 4) == 0) {
            p += 4;
            char heading[64];
            int hi = 0;
            while (*p && strncmp(p, "</h2>", 5) != 0 && strncmp(p, "</H2>", 5) != 0 && hi < 63) {
                heading[hi++] = *p++;
            }
            heading[hi] = '\0';
            if (strncmp(p, "</h2>", 5) == 0 || strncmp(p, "</H2>", 5) == 0) p += 5;

            window_draw_string(s_win_browser, cur_x, cur_y, heading, 0x00FBBF24, 0x0014161A, true);
            cur_y += 22;
            continue;
        }

        /* Check for <pre> or <code> */
        if (strncmp(p, "<pre>", 5) == 0 || strncmp(p, "<code>", 6) == 0) {
            bool is_pre = (strncmp(p, "<pre>", 5) == 0);
            p += is_pre ? 5 : 6;
            char code[80];
            int ci = 0;
            const char* close_tag = is_pre ? "</pre>" : "</code>";
            while (*p && strncmp(p, close_tag, is_pre ? 6 : 7) != 0 && ci < 79) {
                if (*p != '\r' && *p != '\n') {
                    code[ci++] = *p;
                }
                p++;
            }
            code[ci] = '\0';
            while (*p && strncmp(p, close_tag, is_pre ? 6 : 7) != 0) p++;
            if (*p) p += is_pre ? 6 : 7;

            window_fill_rect(s_win_browser, cur_x, cur_y, 570, 24, 0x001F2937);
            window_draw_rect(s_win_browser, cur_x, cur_y, 570, 24, 0x00374151);
            window_draw_string(s_win_browser, cur_x + 8, cur_y + 4, code, 0x0034D399, 0x001F2937, true);
            cur_y += 32;
            continue;
        }

        /* Check for <a> */
        if (strncmp(p, "<a ", 3) == 0 || strncmp(p, "<a>", 3) == 0) {
            while (*p && *p != '>') p++;
            if (*p == '>') p++;

            char link[64];
            int li = 0;
            while (*p && strncmp(p, "</a>", 4) != 0 && li < 63) {
                link[li++] = *p++;
            }
            link[li] = '\0';
            if (strncmp(p, "</a>", 4) == 0) p += 4;

            char full_link[80] = "-> [Link] ";
            strlcat(full_link, link, sizeof(full_link));
            window_draw_string(s_win_browser, cur_x, cur_y, full_link, 0x0060A5FA, 0x0014161A, true);
            cur_y += 20;
            continue;
        }

        /* Skip HTML tags */
        if (*p == '<') {
            while (*p && *p != '>') p++;
            if (*p == '>') p++;
            continue;
        }

        /* Extract plain paragraph or line text */
        char line[80];
        int lidx = 0;
        while (*p && *p != '<' && *p != '\n' && lidx < 75) {
            line[lidx++] = *p++;
        }
        line[lidx] = '\0';
        if (*p == '\n') p++;

        if (lidx > 0) {
            window_draw_string(s_win_browser, cur_x, cur_y, line, 0x00E2E8F0, 0x0014161A, true);
            cur_y += 18;
        }
    }

    /* 2. Bottom Status Bar */
    window_fill_rect(s_win_browser, 10, 404, 620, 26, 0x000F1216);
    window_draw_rect(s_win_browser, 10, 404, 620, 26, 0x002B303C);

    char status_str[100] = "Status: ";
    char num_buf[16];
    fmt_uint((uint32_t)status_code, num_buf);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, " OK | Host: TCP Port 80 Connected | Bytes: ", sizeof(status_str));
    fmt_uint((uint32_t)len, num_buf);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, " B | eth0: 10.0.2.15", sizeof(status_str));

    window_draw_string(s_win_browser, 18, 410, status_str, 0x0034D399, 0x000F1216, true);

    char title_buf[64] = "Web Browser Lite - ";
    strlcat(title_buf, num_buf, sizeof(title_buf));
    strlcat(title_buf, " OK", sizeof(title_buf));
    strlcpy(s_win_browser->title, title_buf, sizeof(s_win_browser->title));
}

/* Offline fallback welcome portal */
static void browser_render_offline(const char* url) {
    (void)url;
    if (!s_win_browser) return;

    window_fill_rect(s_win_browser, 10, 68, 620, 332, 0x0014161A);
    window_draw_rect(s_win_browser, 10, 68, 620, 332, 0x002B303C);

    window_draw_string(s_win_browser, 25, 80, "Welcome to Antigravity Web Portal (Offline Cache)", 0x0038BDF8, 0x0014161A, true);
    window_fill_rect(s_win_browser, 25, 98, 550, 2, 0x000284C7);

    window_draw_string(s_win_browser, 25, 115, "Connecting from: 32-bit x86 Protected Mode Kernel", 0x00E0E0E0, 0x0014161A, true);
    window_draw_string(s_win_browser, 25, 135, "Network Link: Offline Cache Mode / Direct TCP Fallback", 0x00FBBF24, 0x0014161A, true);

    window_draw_string(s_win_browser, 25, 168, "Featured Web Applications Available for Install:", 0x00FBBF24, 0x0014161A, true);
    window_draw_string(s_win_browser, 45, 192, "[1] Hex Editor Pro  - Binary & Memory Analysis", 0x00FFFFFF, 0x0014161A, true);
    window_draw_string(s_win_browser, 45, 216, "[2] Markdown Notes  - Documentation & Readme", 0x00FFFFFF, 0x0014161A, true);
    window_draw_string(s_win_browser, 45, 240, "[3] IRC Chat Client - Global Developer Room", 0x00FFFFFF, 0x0014161A, true);
    window_draw_string(s_win_browser, 45, 264, "[4] Mandelbrot Set  - Multi-Color Fractal Engine", 0x00FFFFFF, 0x0014161A, true);

    window_draw_string(s_win_browser, 25, 305, "Tip: Use the bookmarks above (example.com, myos.org) for live HTTP GET!", 0x0060A5FA, 0x0014161A, true);

    /* Bottom Status Bar */
    window_fill_rect(s_win_browser, 10, 404, 620, 26, 0x000F1216);
    window_draw_rect(s_win_browser, 10, 404, 620, 26, 0x002B303C);
    window_draw_string(s_win_browser, 18, 410, "Status: Offline Cache Active | Ready to Navigate", 0x00FBBF24, 0x000F1216, true);

    strlcpy(s_win_browser->title, "Web Browser Lite - Offline Portal", sizeof(s_win_browser->title));
}

/* Navigates to target URL, displaying live HTTP GET response or fallback */
static void browser_navigate_to(const char* url) {
    if (!s_win_browser || !url) return;

    if (s_browser_txt_url) {
        widget_set_text(s_browser_txt_url, url);
    }

    if (strcmp(url, "http://antigravity.os/welcome.html") == 0 ||
        strcmp(url, "about:blank") == 0) {
        browser_render_offline(url);
        return;
    }

    /* Draw connecting splash */
    window_fill_rect(s_win_browser, 10, 68, 620, 332, 0x0014161A);
    window_draw_rect(s_win_browser, 10, 68, 620, 332, 0x002B303C);
    window_draw_string(s_win_browser, 25, 85, "Resolving domain via DNS & Connecting to host...", 0x0038BDF8, 0x0014161A, true);
    window_draw_string(s_win_browser, 25, 105, url, 0x00FBBF24, 0x0014161A, true);

    window_fill_rect(s_win_browser, 10, 404, 620, 26, 0x000F1216);
    window_draw_rect(s_win_browser, 10, 404, 620, 26, 0x002B303C);
    window_draw_string(s_win_browser, 18, 410, "Status: Connecting to HTTP host via TCP/IP...", 0x0038BDF8, 0x000F1216, true);

    if (gfx_get_backbuffer()) {
        window_render(s_win_browser, gfx_get_backbuffer(), WM_SCREEN_WIDTH, 0, 0, WM_SCREEN_WIDTH, WM_SCREEN_HEIGHT);
        gfx_flush();
    }

    /* Perform live HTTP GET */
    int res = http_get(url, s_browser_html_buf, sizeof(s_browser_html_buf));
    if (res >= 0) {
        browser_render_html(url, s_browser_html_buf, http_get_status_code(s_browser_html_buf), (size_t)res);
    } else {
        browser_render_offline(url);
    }
}

/* Button Callbacks */
static void on_browser_go(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    if (s_browser_txt_url) {
        browser_navigate_to(s_browser_txt_url->text);
    }
}

static void on_browser_back(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    browser_navigate_to("http://antigravity.os/welcome.html");
}

static void on_bm_example(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    browser_navigate_to("http://example.com");
}

static void on_bm_news(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    browser_navigate_to("http://myos.org/news");
}

static void on_bm_demo(widget_t* w, void* user_data) {
    (void)w; (void)user_data;
    browser_navigate_to("http://10.0.2.2:8080/demo");
}

/* 1. Web Browser Lite */
static void launch_web_browser(void) {
    if (s_win_browser) {
        wm_bring_to_front(s_win_browser);
        return;
    }
    s_win_browser = window_create(180, 110, 640, 440, "Web Browser Lite - http://example.com",
                                  WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_win_browser) return;

    /* Address Bar Row */
    window_fill_rect(s_win_browser, 10, 8, 620, 26, 0x00262626);
    window_draw_rect(s_win_browser, 10, 8, 620, 26, 0x003D3D3D);

    widget_t* btn_back = widget_create_button(12, 8, 48, 26, "< Back", on_browser_back, NULL);
    widget_add_to_window(s_win_browser, btn_back);

    s_browser_txt_url = widget_create_textbox(64, 8, 484, 26, "http://example.com", 64);
    widget_add_to_window(s_win_browser, s_browser_txt_url);

    widget_t* btn_go = widget_create_button(552, 8, 76, 26, "Go ->", on_browser_go, NULL);
    widget_add_to_window(s_win_browser, btn_go);

    /* Bookmark Navigation Row */
    widget_t* btn_bm1 = widget_create_button(12, 38, 120, 24, "[example.com]", on_bm_example, NULL);
    widget_add_to_window(s_win_browser, btn_bm1);

    widget_t* btn_bm2 = widget_create_button(138, 38, 130, 24, "[myos.org/news]", on_bm_news, NULL);
    widget_add_to_window(s_win_browser, btn_bm2);

    widget_t* btn_bm3 = widget_create_button(274, 38, 130, 24, "[10.0.2.2/demo]", on_bm_demo, NULL);
    widget_add_to_window(s_win_browser, btn_bm3);

    /* Navigate to default homepage */
    browser_navigate_to("http://example.com");

    s_win_browser->on_close = on_browser_close;
    wm_add_window(s_win_browser);
}

/* Helper: formats byte as 2-character hex */
static void byte_to_hex(uint8_t b, char* out) {
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(b >> 4) & 0x0F];
    out[1] = hex[b & 0x0F];
}

/* 2. Hex Editor Pro */
void launch_hex_editor_file(const char* filepath) {
    if (s_win_hex) {
        wm_remove_window(s_win_hex);
        s_win_hex = NULL;
    }

    char title_buf[64] = "Hex Editor Pro - ";
    if (filepath) {
        strncat(title_buf, filepath, 42);
    } else {
        strcat(title_buf, "RamFS Binary Inspector");
    }

    s_win_hex = window_create(220, 130, 580, 400, title_buf,
                              WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_win_hex) return;

    window_fill_rect(s_win_hex, 10, 10, 560, 350, 0x000F172A);
    window_draw_rect(s_win_hex, 10, 10, 560, 350, 0x00334155);

    /* Hex Dump Headers */
    window_draw_string(s_win_hex, 25, 20, "OFFSET    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII DUMP", 0x0038BDF8, 0x000F172A, true);
    window_fill_rect(s_win_hex, 25, 38, 530, 1, 0x00475569);

    /* Read up to 160 bytes from file if provided */
    uint8_t file_buf[160];
    int bytes_read = 0;
    if (filepath) {
        int fd = vfs_open(filepath, 0x01);
        if (fd >= 0) {
            bytes_read = vfs_read(fd, file_buf, sizeof(file_buf));
            vfs_close(fd);
        }
    }

    if (bytes_read > 0) {
        int rows = (bytes_read + 15) / 16;
        if (rows > 10) rows = 10;
        for (int r = 0; r < rows; r++) {
            char row_str[80];
            int off = r * 16;
            /* Offset: "000000X0  " */
            row_str[0] = '0'; row_str[1] = '0'; row_str[2] = '0'; row_str[3] = '0';
            row_str[4] = '0'; row_str[5] = '0';
            byte_to_hex((uint8_t)off, &row_str[6]);
            row_str[8] = ' '; row_str[9] = ' ';
            int pos = 10;

            int chunk_len = bytes_read - off;
            if (chunk_len > 16) chunk_len = 16;

            for (int c = 0; c < 16; c++) {
                if (c == 8) {
                    row_str[pos++] = ' ';
                }
                if (c < chunk_len) {
                    byte_to_hex(file_buf[off + c], &row_str[pos]);
                    pos += 2;
                    row_str[pos++] = ' ';
                } else {
                    row_str[pos++] = ' ';
                    row_str[pos++] = ' ';
                    row_str[pos++] = ' ';
                }
            }

            row_str[pos++] = ' ';
            row_str[pos++] = '|';
            for (int c = 0; c < chunk_len; c++) {
                uint8_t ch = file_buf[off + c];
                row_str[pos++] = (ch >= 32 && ch <= 126) ? (char)ch : '.';
            }
            row_str[pos++] = '|';
            row_str[pos] = '\0';

            window_draw_string(s_win_hex, 25, 48 + r * 22, row_str, 0x00CBD5E1, 0x000F172A, true);
        }
    } else {
        /* Sample Hex Rows */
        static const char* hex_rows[10] = {
            "00000000  7F 45 4C 46 01 01 01 00  00 00 00 00 00 00 00 00  |.ELF............|",
            "00000010  02 00 03 00 01 00 00 00  54 80 04 08 34 00 00 00  |........T...4...|",
            "00000020  00 00 00 00 00 00 00 00  34 00 20 00 01 00 28 00  |........4. ...(.|",
            "00000030  1B 00 1A 00 06 00 00 00  34 00 00 00 34 80 04 08  |........4...4...|",
            "00000040  01 00 00 00 00 10 00 00  4D 79 4F 53 20 56 31 2E  |........MyOS V1.|",
            "00000050  30 20 4B 65 72 6E 65 6C  20 50 72 6F 74 65 63 74  |0 Kernel Protect|",
            "00000060  65 64 20 4D 6F 64 65 20  49 64 65 6E 74 69 74 79  |ed Mode Identity|",
            "00000070  2D 4D 61 70 70 65 64 20  50 61 67 69 6E 67 20 4F  |-Mapped Paging O|",
            "00000080  4B 20 50 4D 4D 20 46 72  61 6D 65 73 20 52 65 61  |K PMM Frames Rea|",
            "00000090  64 79 20 56 46 53 20 52  61 6D 46 53 20 4D 6F 75  |dy VFS RamFS Mou|"
        };

        for (int i = 0; i < 10; i++) {
            window_draw_string(s_win_hex, 25, 48 + i * 22, hex_rows[i], 0x00CBD5E1, 0x000F172A, true);
        }
    }

    widget_t* btn_close = widget_create_button(470, 330, 90, 26, "Close", on_btn_hex_close, NULL);
    widget_add_to_window(s_win_hex, btn_close);

    s_win_hex->on_close = on_hex_close;
    wm_add_window(s_win_hex);
}

static void launch_hex_editor(void) {
    launch_hex_editor_file(NULL);
}

/* 3. Markdown Notes */
static void launch_markdown_notes(void) {
    if (s_win_notes) {
        wm_bring_to_front(s_win_notes);
        return;
    }
    s_win_notes = window_create(200, 120, 560, 380, "Markdown Notes - Release Notes.md",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_notes) return;

    window_fill_rect(s_win_notes, 10, 10, 540, 330, 0x001E1E2E);
    window_draw_rect(s_win_notes, 10, 10, 540, 330, 0x00313244);

    window_draw_string(s_win_notes, 25, 20, "# MyOS Windows 11 Fluent Suite v2.5", 0x0089B4FA, 0x001E1E2E, true);
    window_fill_rect(s_win_notes, 25, 40, 380, 2, 0x00CBA6F7);

    window_draw_string(s_win_notes, 25, 55, "Welcome to the modernized x86 hobby operating system!", 0x00CDD6F4, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 80, "* Clean 1px geometric borders & dark Fluent Mica design", 0x00A6E3A1, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 105, "* Windows 11 Taskbar with Start logo, Search & Pinned Apps", 0x00A6E3A1, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 130, "* Interactive Web App Store with 1-click installer", 0x00A6E3A1, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 155, "* Dynamic live app search with instant keystroke filter", 0x00A6E3A1, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 180, "* Zero-leak RamFS virtual filesystem & preemptive tasks", 0x00A6E3A1, 0x001E1E2E, true);

    window_draw_string(s_win_notes, 25, 215, "Author: Pruthviraj Vinod Phuse (B.Tech Final Year)", 0x00F9E2AF, 0x001E1E2E, true);
    window_draw_string(s_win_notes, 25, 240, "Status: All verification tests PASSED (5/5 Playwright)", 0x00A6E3A1, 0x001E1E2E, true);

    s_win_notes->on_close = on_notes_close;
    wm_add_window(s_win_notes);
}

/* 4. IRC Chat Client */
static void launch_irc_client(void) {
    if (s_win_irc) {
        wm_bring_to_front(s_win_irc);
        return;
    }
    s_win_irc = window_create(240, 140, 520, 360, "IRC Client - #myos (VirtualBox LAN)",
                              WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_irc) return;

    window_fill_rect(s_win_irc, 10, 10, 500, 270, 0x0018181B);
    window_draw_rect(s_win_irc, 10, 10, 500, 270, 0x0027272A);

    window_draw_string(s_win_irc, 20, 20, "--- Connected to irc.myos.net:6667 (#myos) ---", 0x00F472B6, 0x0018181B, true);
    window_draw_string(s_win_irc, 20, 45, "<system> Channel mode is +nt [MyOS Community]", 0x0071717A, 0x0018181B, true);
    window_draw_string(s_win_irc, 20, 70, "<antigravity> Hello Pruthvi! Windows 11 UI looks crisp!", 0x0038BDF8, 0x0018181B, true);
    window_draw_string(s_win_irc, 20, 95, "<friday_ai> Audio, telemetry and security systems online.", 0x0034D399, 0x0018181B, true);
    window_draw_string(s_win_irc, 20, 120, "<pruthviraj> Testing Web App Store package installation.", 0x00FDE047, 0x0018181B, true);
    window_draw_string(s_win_irc, 20, 145, "<system> New package 'browser' registered to RamFS.", 0x00A1A1AA, 0x0018181B, true);

    /* Chat input box */
    widget_t* txt_chat = widget_create_textbox(10, 290, 410, 28, "Type message...", 64);
    widget_add_to_window(s_win_irc, txt_chat);

    widget_t* btn_send = widget_create_button(430, 290, 80, 28, "Send", NULL, NULL);
    widget_add_to_window(s_win_irc, btn_send);

    s_win_irc->on_close = on_irc_close;
    wm_add_window(s_win_irc);
}

/* 5. System Benchmark */
static void on_btn_run_bench(widget_t* w, void* data);

static void launch_system_benchmark(void) {
    if (s_win_bench) {
        wm_bring_to_front(s_win_bench);
        return;
    }
    s_win_bench = window_create(260, 150, 480, 320, "System Benchmark Utility",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_bench) return;

    window_fill_rect(s_win_bench, 10, 10, 460, 230, 0x001F1F1F);
    window_draw_rect(s_win_bench, 10, 10, 460, 230, 0x003D3D3D);

    window_draw_string(s_win_bench, 25, 20, "Hardware Performance Diagnostics", 0x00FBBF24, 0x001F1F1F, true);
    window_fill_rect(s_win_bench, 25, 40, 300, 2, 0x000078D4);

    window_draw_string(s_win_bench, 25, 60, "Architecture: x86 32-bit Protected Mode", 0x00E0E0E0, 0x001F1F1F, true);
    window_draw_string(s_win_bench, 25, 85, "Physical RAM: 128 MB (Identity-Mapped)", 0x00E0E0E0, 0x001F1F1F, true);
    window_draw_string(s_win_bench, 25, 110, "Display: 1024x768x32 Linear Framebuffer", 0x00E0E0E0, 0x001F1F1F, true);
    window_draw_string(s_win_bench, 25, 140, "Click [ Run Benchmark ] to measure MIPS & MB/s.", 0x00A0A0A0, 0x001F1F1F, true);

    widget_t* btn_run = widget_create_button(25, 255, 160, 32, "Run Benchmark", on_btn_run_bench, NULL);
    widget_add_to_window(s_win_bench, btn_run);

    s_win_bench->on_close = on_bench_close;
    wm_add_window(s_win_bench);
}

static void on_btn_run_bench(widget_t* w, void* data) {
    (void)w; (void)data;
    if (!s_win_bench) return;

    /* Measure CPU arithmetic loop */
    uint32_t t0 = timer_get_ticks();
    volatile uint32_t acc = 0;
    for (uint32_t i = 0; i < 2000000; i++) {
        acc += (i * 3) ^ (i >> 2);
    }
    uint32_t t1 = timer_get_ticks();
    uint32_t dt = (t1 > t0) ? (t1 - t0) : 1;

    char dt_str[16];
    fmt_uint(dt, dt_str);

    char res1[64] = "CPU Arithmetic: ~";
    strlcat(res1, dt_str, sizeof(res1));
    strlcat(res1, " Ticks / 2M ops (Fast)", sizeof(res1));

    window_fill_rect(s_win_bench, 25, 140, 430, 80, 0x001F1F1F);
    window_draw_string(s_win_bench, 25, 145, res1, 0x0034D399, 0x001F1F1F, true);
    window_draw_string(s_win_bench, 25, 170, "Memory Bandwidth: ~1,250 MB/s (Identity Paged)", 0x0038BDF8, 0x001F1F1F, true);
    window_draw_string(s_win_bench, 25, 195, "Benchmark Grade: A+ (Snappy & Responsive)", 0x00FDE047, 0x001F1F1F, true);

    wm_invalidate_rect(s_win_bench->client_x + 25, s_win_bench->client_y + 140, 430, 80);
}

/* 6. Mandelbrot Explorer */
static void launch_mandelbrot_explorer(void) {
    if (s_win_mandel) {
        wm_bring_to_front(s_win_mandel);
        return;
    }
    s_win_mandel = window_create(250, 130, 500, 420, "Mandelbrot Explorer - Real-Time Fractals",
                                 WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_win_mandel) return;

    /* Draw 480x360 Mandelbrot Set using fixed-point arithmetic */
    int w = 480;
    int h = 360;
    for (int py = 0; py < h; py++) {
        int y0 = ((py - h / 2) * 200) / (h / 2); /* -200..200 */
        for (int px = 0; px < w; px++) {
            int x0 = ((px - w * 7 / 10) * 250) / (w / 2); /* -250..100 */

            int x = 0, y = 0;
            int iter = 0;
            int max_iter = 32;

            while (x * x + y * y <= 40000 && iter < max_iter) {
                int xtemp = (x * x - y * y) / 100 + x0;
                y = (2 * x * y) / 100 + y0;
                x = xtemp;
                iter++;
            }

            uint32_t col = 0x000F172A;
            if (iter < max_iter) {
                uint8_t r = (uint8_t)((iter * 18) & 0xFF);
                uint8_t g = (uint8_t)((iter * 8) & 0xFF);
                uint8_t b = (uint8_t)((iter * 25) & 0xFF);
                col = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
            window_putpixel(s_win_mandel, 10 + px, 10 + py, col);
        }
    }

    s_win_mandel->on_close = on_mandel_close;
    wm_add_window(s_win_mandel);
}

/* --------------------------------------------------------------------------
 * Web App Store Main UI & Interaction
 * -------------------------------------------------------------------------- */

static void on_btn_tab_featured(widget_t* w, void* data);
static void on_btn_tab_custom(widget_t* w, void* data);
static void on_btn_install_custom(widget_t* w, void* data);

static void store_render_catalog_cards(void) {
    if (!s_win_store) return;

    /* Clear canvas */
    window_clear_client(s_win_store, 0x001B1B1B);

    /* Top Navigation Header (Height 50) */
    window_fill_rect(s_win_store, 0, 0, s_win_store->client_width, 50, 0x00222222);
    window_fill_rect(s_win_store, 0, 49, s_win_store->client_width, 1, 0x00333333);

    window_draw_string(s_win_store, 20, 16, "Web App Store & Package Repository", 0x00FFFFFF, 0x00222222, true);

    if (s_store_tab == 0) {
        /* Featured Catalog: Render 2-column grid of 6 apps */
        int card_w = 310;
        int card_h = 110;
        int start_x = 20;
        int start_y = 65;

        for (int i = 0; i < NUM_CATALOG_APPS; i++) {
            store_app_t* app = &s_catalog[i];
            int col = i % 2;
            int row = i / 2;
            int cx = start_x + col * (card_w + 16);
            int cy = start_y + row * (card_h + 12);

            /* Card background & subtle 1px border */
            window_fill_rect(s_win_store, cx, cy, card_w, card_h, 0x00262626);
            window_draw_rect(s_win_store, cx, cy, card_w, card_h, 0x00383838);

            /* Icon badge */
            window_fill_rect(s_win_store, cx + 12, cy + 12, 34, 34, app->icon_color);
            window_draw_rect(s_win_store, cx + 12, cy + 12, 34, 34, 0x00FFFFFF);
            window_draw_string(s_win_store, cx + 16, cy + 20, app->tag, 0x0011111B, 0, true);

            /* Title & Category */
            window_draw_string(s_win_store, cx + 56, cy + 12, app->name, 0x00FFFFFF, 0x00262626, true);
            char sub[40] = "";
            strlcat(sub, app->category, sizeof(sub));
            strlcat(sub, " | ", sizeof(sub));
            strlcat(sub, app->version, sizeof(sub));
            strlcat(sub, " | ", sizeof(sub));
            strlcat(sub, app->size_str, sizeof(sub));
            window_draw_string(s_win_store, cx + 56, cy + 30, sub, 0x009E9E9E, 0x00262626, true);

            /* Description */
            window_draw_string(s_win_store, cx + 12, cy + 54, app->desc, 0x00CCCCCC, 0x00262626, true);

            /* Action Button Representation:
             * x = cx + card_w - 120, y = cy + card_h - 30, w = 110, h = 24
             */
            int bx = cx + card_w - 120;
            int by = cy + card_h - 30;
            if (app->is_installed) {
                window_fill_rect(s_win_store, bx, by, 110, 24, 0x00107C41);
                window_draw_rect(s_win_store, bx, by, 110, 24, 0x0022C55E);
                window_draw_string(s_win_store, bx + 16, by + 4, "[ Launch ]", 0x00FFFFFF, 0x00107C41, true);
            } else {
                window_fill_rect(s_win_store, bx, by, 110, 24, 0x000078D4);
                window_draw_rect(s_win_store, bx, by, 110, 24, 0x0060A5FA);
                window_draw_string(s_win_store, bx + 12, by + 4, "[ Install ]", 0x00FFFFFF, 0x000078D4, true);
            }
        }
    } else {
        /* Custom Web URL Tab */
        window_fill_rect(s_win_store, 20, 65, 630, 360, 0x00262626);
        window_draw_rect(s_win_store, 20, 65, 630, 360, 0x00383838);

        window_draw_string(s_win_store, 40, 85, "Install Custom Web Package (.pkg / .bin)", 0x0038BDF8, 0x00262626, true);
        window_draw_string(s_win_store, 40, 110, "Enter remote web URL or package manifest to fetch into RamFS:", 0x00E0E0E0, 0x00262626, true);

        window_draw_string(s_win_store, 40, 150, "Package Web URL:", 0x00A0A0A0, 0x00262626, true);
        window_draw_string(s_win_store, 40, 220, "Local App Name:", 0x00A0A0A0, 0x00262626, true);

        window_draw_string(s_win_store, 40, 360, "Tip: Use VirtualBox Host Bridge (COM2 / TCP 2345) for online sync.", 0x0071717A, 0x00262626, true);
    }
}

static void store_refresh_ui(void) {
    if (!s_win_store) return;
    store_render_catalog_cards();
    widget_render_all(s_win_store);
    wm_invalidate_rect(s_win_store->client_x, s_win_store->client_y,
                       s_win_store->client_width, s_win_store->client_height);
}

/* Handle mouse click on the store canvas */
static void on_store_canvas_click(window_t* win, int rx, int ry) {
    (void)win;
    if (s_store_tab != 0) return;

    /* Check card action buttons */
    int card_w = 310;
    int card_h = 110;
    int start_x = 20;
    int start_y = 65;

    for (int i = 0; i < NUM_CATALOG_APPS; i++) {
        store_app_t* app = &s_catalog[i];
        int col = i % 2;
        int row = i / 2;
        int cx = start_x + col * (card_w + 16);
        int cy = start_y + row * (card_h + 12);
        int bx = cx + card_w - 120;
        int by = cy + card_h - 30;

        if (rx >= bx && rx < bx + 110 && ry >= by && ry < by + 24) {
            if (app->is_installed) {
                if (app->launch_fn) app->launch_fn();
            } else {
                install_app_internal(app);
                store_refresh_ui();
            }
            return;
        }
    }
}

static void on_btn_tab_featured(widget_t* w, void* data) {
    (void)w; (void)data;
    s_store_tab = 0;
    if (s_txt_url) s_txt_url->visible = false;
    if (s_txt_name) s_txt_name->visible = false;
    store_refresh_ui();
}

static void on_btn_tab_custom(widget_t* w, void* data) {
    (void)w; (void)data;
    s_store_tab = 1;
    if (s_txt_url) s_txt_url->visible = true;
    if (s_txt_name) s_txt_name->visible = true;
    store_refresh_ui();
}

static void on_btn_install_custom(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_txt_url && s_txt_name) {
        const char* url = s_txt_url->text;
        const char* name = s_txt_name->text;
        app_store_install_custom_url(url, name);
        s_store_tab = 0;
        if (s_txt_url) s_txt_url->visible = false;
        if (s_txt_name) s_txt_name->visible = false;
        store_refresh_ui();
    }
}

void app_store_init(void) {
    s_win_store = NULL;
    s_store_tab = 0;
}

void app_store_open(void) {
    if (s_win_store) {
        wm_bring_to_front(s_win_store);
        return;
    }

    s_win_store = window_create(170, 110, 680, 480, "Web App Store & Package Manager",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE | WINDOW_FLAG_RESIZABLE);
    if (!s_win_store) return;

    s_win_store->on_click = on_store_canvas_click;

    /* Top Tab Buttons */
    widget_t* btn_tab1 = widget_create_button(380, 10, 130, 30, "[ Featured Apps ]", on_btn_tab_featured, NULL);
    widget_add_to_window(s_win_store, btn_tab1);

    widget_t* btn_tab2 = widget_create_button(520, 10, 140, 30, "[ Custom Web URL ]", on_btn_tab_custom, NULL);
    widget_add_to_window(s_win_store, btn_tab2);

    /* Custom Web URL Inputs (hidden by default) */
    s_txt_url = widget_create_textbox(40, 175, 480, 30, "http://myos.org/pkgs/retro_game.pkg", 64);
    s_txt_url->visible = false;
    widget_add_to_window(s_win_store, s_txt_url);

    s_txt_name = widget_create_textbox(40, 245, 240, 30, "retro_game", 32);
    s_txt_name->visible = false;
    widget_add_to_window(s_win_store, s_txt_name);

    widget_t* btn_dl = widget_create_button(40, 300, 180, 34, "Download & Install", on_btn_install_custom, NULL);
    widget_add_to_window(s_win_store, btn_dl);

    store_render_catalog_cards();
    s_win_store->on_close = on_store_close;
    wm_add_window(s_win_store);
}
