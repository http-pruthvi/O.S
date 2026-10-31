#include "apps/photos.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "fs/vfs.h"
#include "gfx/gfx.h"
#include "lib/string.h"
#include "lib/kprintf.h"
#include "mm/heap.h"
#include "drivers/serial.h"

/*
 * photos.c:
 * Graphical Photos & Image Viewer Application for MyOS.
 *
 * Why this is needed:
 * Desktop environments require an intuitive, dedicated image viewer capable of
 * opening image files, inspecting pixel dimensions, zooming in for detail analysis,
 * and applying visual manipulation filters in real time.
 *
 * Features:
 * 1. Supports Netpbm Portable Pixmap (P3 ASCII and P6 Binary) formats.
 * 2. Embedded gallery featuring procedural demo images (Sunset Landscape, Retro PC Icon).
 * 3. Dynamic RamFS directory scanner that auto-loads any user `.ppm` images.
 * 4. Image zoom engine scaling from 1x up to 6x pixel blocks.
 * 5. Real-time Invert and Luminance Grayscale color filtering.
 * 6. Full mouse button and keyboard shortcut navigation.
 */

#define PHOTOS_WIN_WIDTH       420
#define PHOTOS_WIN_HEIGHT      340
#define PHOTOS_MAX_GALLERY     16
#define PHOTOS_MIN_ZOOM        1
#define PHOTOS_MAX_ZOOM        6

/* Viewport Canvas Dimensions inside Window Client Area */
#define PHOTOS_VP_X            6
#define PHOTOS_VP_Y            38
#define PHOTOS_VP_W            404
#define PHOTOS_VP_H            246
#define PHOTOS_STATUS_Y        288

/* Internal representation of a loaded gallery image */
typedef struct {
    char      name[32];        /* Filename or title label */
    int       width;           /* Original image width in pixels */
    int       height;          /* Original image height in pixels */
    uint32_t* pixels;          /* 32-bit RGB pixel buffer (0x00RRGGBB) */
} photo_entry_t;

/* Application State */
static window_t*      s_photos_win       = NULL;
static photo_entry_t  s_gallery[PHOTOS_MAX_GALLERY];
static int            s_gallery_count    = 0;
static int            s_current_index    = 0;
static int            s_zoom             = 1;
static bool           s_filter_invert    = false;
static bool           s_filter_grayscale = false;

/* Helper: converts an integer value to a null-terminated string */
static void int_to_str(int32_t val, char* buf) {
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    int idx = 0;
    if (val < 0) {
        buf[idx++] = '-';
        val = -val;
    }
    char tmp[16];
    int t = 0;
    while (val > 0) {
        tmp[t++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = t - 1; i >= 0; i--) {
        buf[idx++] = tmp[i];
    }
    buf[idx] = '\0';
}

/* Helper: appends unsigned integer string representation and advances pointer */
static char* append_uint(char* p, uint32_t val) {
    if (val == 0) {
        *p++ = '0';
        return p;
    }
    char temp[12];
    int t = 0;
    while (val > 0) {
        temp[t++] = (char)('0' + (val % 10));
        val /= 10;
    }
    while (t > 0) {
        *p++ = temp[--t];
    }
    return p;
}

/*
 * ==============================================================================
 * Built-in Procedural Demo Imagery
 * ==============================================================================
 */

/*
 * generate_sunset_image:
 * Procedurally generates Image 0: Colorful Sunset / Gradient scene with geometric
 * sun, dual-layer mountain silhouettes, and shimmering water reflections.
 */
static uint32_t* generate_sunset_image(int* out_w, int* out_h) {
    int w = 96;
    int h = 64;
    uint32_t* pix = (uint32_t*)kmalloc((size_t)(w * h) * sizeof(uint32_t));
    if (!pix) return NULL;

    *out_w = w;
    *out_h = h;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t col = 0;

            if (y < 46) {
                /* 1. Sky: Vertical color gradient from indigo (top) to warm orange/gold (horizon) */
                int t = (y * 255) / 45;
                int r = 0x24 + ((0xFF - 0x24) * t) / 255;
                int g = 0x0A + ((0x98 - 0x0A) * t) / 255;
                int b = 0x46 + ((0x10 - 0x46) * t) / 255;
                if (b < 0) b = 0;

                /* 2. Sun: Circular glowing orb centered at (48, 28) with radius 12 */
                int dx = x - 48;
                int dy = y - 28;
                int d2 = dx * dx + dy * dy;
                if (d2 <= 144) {
                    col = 0x00FFF490; /* Bright core */
                } else if (d2 <= 225) {
                    col = 0x00FFA834; /* Corona halo */
                } else {
                    col = (uint32_t)((r << 16) | (g << 8) | b);
                }

                /* 3. Distant mountain peaks (purple-gray dusk silhouette) */
                int dist1 = (x > 24) ? (x - 24) : (24 - x);
                int m1_y = 38 - (14 - dist1 / 2);
                int dist2 = (x > 68) ? (x - 68) : (68 - x);
                int m2_y = 40 - (12 - dist2 / 2);
                if (y >= m1_y && y < 46) {
                    col = 0x004A265E;
                } else if (y >= m2_y && y < 46) {
                    col = 0x003D1F50;
                }

                /* 4. Foreground obsidian ridge (near mountain peak at x=44) */
                int dist3 = (x > 44) ? (x - 44) : (44 - x);
                int m3_y = 44 - (10 - dist3 / 2);
                if (y >= m3_y && y < 46) {
                    col = 0x001C122C;
                }
            } else {
                /* 5. Reflective lake water with shimmering sun glare at horizon */
                int t = ((y - 46) * 255) / (63 - 46);
                int r = 0x30 - (0x1C * t) / 255;
                int g = 0x14 - (0x0C * t) / 255;
                int b = 0x48 - (0x28 * t) / 255;

                /* Water ripple reflection aligned with sun position */
                if (x >= 40 && x <= 56) {
                    if ((x + y) % 2 == 0) {
                        r += 0x88;
                        g += 0x58;
                        b += 0x14;
                    }
                }
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
                col = (uint32_t)((r << 16) | (g << 8) | b);
            }

            pix[y * w + x] = col;
        }
    }
    return pix;
}

/*
 * generate_retro_pc_image:
 * Procedurally generates Image 1: Retro 8-bit Computer Icon / OS Logo with
 * classic 90s CRT monitor, glowing cyan terminal prompt, keyboard, and mouse.
 */
static uint32_t* generate_retro_pc_image(int* out_w, int* out_h) {
    int w = 48;
    int h = 48;
    uint32_t* pix = (uint32_t*)kmalloc((size_t)(w * h) * sizeof(uint32_t));
    if (!pix) return NULL;

    *out_w = w;
    *out_h = h;

    /* Fill background with dark navy canvas */
    for (int i = 0; i < w * h; i++) {
        pix[i] = 0x001B202A;
    }

    /* Outer monitor bezel: x=6..41, y=4..31 (Classic beige plastic) */
    for (int y = 4; y <= 31; y++) {
        for (int x = 6; x <= 41; x++) {
            uint32_t col = 0x00D0D4DA;
            if (x == 6 || y == 4) col = 0x00EAEFF5;  /* 3D highlight */
            if (x == 41 || y == 31) col = 0x0090949C; /* 3D shadow */
            pix[y * w + x] = col;
        }
    }

    /* Inner CRT screen: x=10..37, y=8..27 (Deep concave phosphor glass) */
    for (int y = 8; y <= 27; y++) {
        for (int x = 10; x <= 37; x++) {
            uint32_t col = 0x000F1722;
            if (x == 10 || y == 8) col = 0x00080D14; /* Inset bezel shadow */
            pix[y * w + x] = col;
        }
    }

    /* Glowing terminal prompt arrow "_>" in phosphor green */
    for (int x = 13; x <= 16; x++) pix[12 * w + x] = 0x0020E080;
    pix[11 * w + 15] = 0x0020E080;
    pix[13 * w + 15] = 0x0020E080;

    /* "MYOS" glowing logo across screen lines */
    /* Letter M (x=19..23, y=14..19) */
    for (int y = 14; y <= 19; y++) {
        pix[y * w + 19] = 0x0033CCFF;
        pix[y * w + 23] = 0x0033CCFF;
    }
    pix[15 * w + 20] = 0x0033CCFF;
    pix[16 * w + 21] = 0x0033CCFF;
    pix[15 * w + 22] = 0x0033CCFF;

    /* Letter O (x=25..29, y=14..19) */
    for (int y = 14; y <= 19; y++) {
        pix[y * w + 25] = 0x0033CCFF;
        pix[y * w + 29] = 0x0033CCFF;
    }
    for (int x = 25; x <= 29; x++) {
        pix[14 * w + x] = 0x0033CCFF;
        pix[19 * w + x] = 0x0033CCFF;
    }

    /* Letter S (x=31..34, y=14..19) */
    for (int x = 31; x <= 34; x++) {
        pix[14 * w + x] = 0x0033CCFF;
        pix[16 * w + x] = 0x0033CCFF;
        pix[19 * w + x] = 0x0033CCFF;
    }
    pix[15 * w + 31] = 0x0033CCFF;
    pix[17 * w + 34] = 0x0033CCFF;
    pix[18 * w + 34] = 0x0033CCFF;

    /* Blinking green prompt underline cursor */
    for (int x = 14; x <= 20; x++) {
        pix[23 * w + x] = 0x0055FF55;
    }

    /* Monitor neck & stand swivel: x=21..26, y=32..35 */
    for (int y = 32; y <= 35; y++) {
        for (int x = 21; x <= 26; x++) {
            pix[y * w + x] = 0x0080848C;
        }
    }

    /* Monitor base plate: x=16..31, y=35..36 */
    for (int y = 35; y <= 36; y++) {
        for (int x = 16; x <= 31; x++) {
            pix[y * w + x] = 0x00A0A4AC;
        }
    }

    /* Mechanical keyboard chassis: x=8..39, y=38..44 */
    for (int y = 38; y <= 44; y++) {
        for (int x = 8; x <= 39; x++) {
            uint32_t col = 0x00C4C8D0;
            if (x == 8 || y == 38) col = 0x00E4E8F0;
            if (x == 39 || y == 44) col = 0x00787C84;
            pix[y * w + x] = col;
        }
    }

    /* Keyboard key caps */
    for (int kx = 10; kx <= 37; kx += 3) {
        pix[40 * w + kx] = 0x004A4E56;
        pix[42 * w + kx] = 0x004A4E56;
    }
    /* Spacebar */
    for (int kx = 18; kx <= 29; kx++) {
        pix[42 * w + kx] = 0x005E646E;
    }

    /* Two-button desktop mouse: x=42..45, y=40..44 */
    for (int y = 40; y <= 44; y++) {
        for (int x = 42; x <= 45; x++) {
            pix[y * w + x] = 0x00B0B4BC;
        }
    }
    pix[40 * w + 43] = 0x0040444C; /* Split button groove */

    return pix;
}

/*
 * ==============================================================================
 * Robust Netpbm PPM Image Parser (P3 ASCII and P6 Binary)
 * ==============================================================================
 */

/* Helper: skips whitespace and inline '#' comments */
static const uint8_t* ppm_skip_ws_comments(const uint8_t* p, const uint8_t* end) {
    while (p < end) {
        if (*p == '#') {
            /* Skip until end of current line */
            while (p < end && *p != '\n' && *p != '\r') {
                p++;
            }
        } else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        } else {
            break;
        }
    }
    return p;
}

/* Helper: parses an unsigned decimal integer with overflow protection */
static const uint8_t* ppm_read_uint(const uint8_t* p, const uint8_t* end, uint32_t* out_val) {
    p = ppm_skip_ws_comments(p, end);
    if (p >= end || *p < '0' || *p > '9') {
        return NULL;
    }
    uint32_t val = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (val > 1000000) return NULL; /* Guard against malicious integer wrap */
        val = val * 10 + (*p - '0');
        p++;
    }
    *out_val = val;
    return p;
}

/*
 * ppm_parse:
 * Robust, memory-safe parser for Netpbm PPM image streams.
 * Handles both P3 (ASCII) and P6 (Binary) encodings, header comments,
 * and guards against corrupt or truncated payload buffers.
 */
static uint32_t* ppm_parse(const uint8_t* buf, uint32_t size, int* out_w, int* out_h) {
    if (!buf || size < 8) return NULL;

    const uint8_t* p = buf;
    const uint8_t* end = buf + size;

    /* Verify Netpbm magic identifier: 'P3' or 'P6' */
    if (p[0] != 'P' || (p[1] != '3' && p[1] != '6')) {
        return NULL;
    }
    int format = p[1] - '0';
    p += 2;

    /* Parse width, height, and maximum channel intensity */
    uint32_t width = 0, height = 0, maxval = 0;
    p = ppm_read_uint(p, end, &width);
    if (!p) return NULL;
    p = ppm_read_uint(p, end, &height);
    if (!p) return NULL;
    p = ppm_read_uint(p, end, &maxval);
    if (!p) return NULL;

    /* Validate dimensions to ensure memory safety */
    if (width == 0 || width > 1024 || height == 0 || height > 768 || maxval == 0 || maxval > 65535) {
        return NULL;
    }
    uint32_t total_pixels = width * height;
    if (total_pixels > 1024 * 768) {
        return NULL;
    }

    /* Allocate 32-bit RGB pixel buffer in kernel heap */
    uint32_t* pixels = (uint32_t*)kmalloc((size_t)total_pixels * sizeof(uint32_t));
    if (!pixels) return NULL;

    if (format == 6) {
        /* Format P6: Binary raster data */
        /* Netpbm specification requires single whitespace byte after maxval */
        if (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
            if (*p == '\r' && (p + 1 < end) && *(p + 1) == '\n') {
                p += 2;
            } else {
                p++;
            }
        }

        for (uint32_t i = 0; i < total_pixels; i++) {
            if (p + 3 > end) {
                /* Truncated binary stream: pad remaining pixels safely */
                while (i < total_pixels) pixels[i++] = 0;
                break;
            }
            uint32_t r = *p++;
            uint32_t g = *p++;
            uint32_t b = *p++;
            if (maxval != 255) {
                r = (r * 255) / maxval;
                g = (g * 255) / maxval;
                b = (b * 255) / maxval;
            }
            pixels[i] = (r << 16) | (g << 8) | b;
        }
    } else {
        /* Format P3: ASCII decimal tokens */
        for (uint32_t i = 0; i < total_pixels; i++) {
            uint32_t r = 0, g = 0, b = 0;
            p = ppm_read_uint(p, end, &r);
            if (!p) {
                while (i < total_pixels) pixels[i++] = 0;
                break;
            }
            p = ppm_read_uint(p, end, &g);
            if (!p) {
                while (i < total_pixels) pixels[i++] = 0;
                break;
            }
            p = ppm_read_uint(p, end, &b);
            if (!p) {
                while (i < total_pixels) pixels[i++] = 0;
                break;
            }

            if (maxval != 255) {
                r = (r * 255) / maxval;
                g = (g * 255) / maxval;
                b = (b * 255) / maxval;
            }
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            pixels[i] = (r << 16) | (g << 8) | b;
        }
    }

    *out_w = (int)width;
    *out_h = (int)height;
    return pixels;
}

/*
 * read_ppm_from_vfs:
 * Loads and parses a PPM file located on the Virtual File System (RamFS).
 */
static uint32_t* read_ppm_from_vfs(const char* filepath, int* out_w, int* out_h) {
    int fd = vfs_open(filepath, 0x01); /* Read-only mode */
    if (fd < 0) return NULL;

    uint32_t file_size = vfs_tell_size(fd);
    if (file_size < 8 || file_size > 1024 * 1024) {
        vfs_close(fd);
        return NULL;
    }

    uint8_t* raw_buf = (uint8_t*)kmalloc((size_t)file_size + 1);
    if (!raw_buf) {
        vfs_close(fd);
        return NULL;
    }

    int bytes_read = vfs_read(fd, raw_buf, file_size);
    vfs_close(fd);

    if (bytes_read <= 0) {
        kfree(raw_buf);
        return NULL;
    }

    raw_buf[bytes_read] = '\0';
    uint32_t* pixels = ppm_parse(raw_buf, (uint32_t)bytes_read, out_w, out_h);
    kfree(raw_buf);
    return pixels;
}

/*
 * ==============================================================================
 * RamFS Pre-seeding
 * ==============================================================================
 */

/*
 * photos_preseed_images:
 * Creates sample PPM image files in RamFS ("/sample.ppm" and "/avatar.ppm")
 * if they are not already present in the filesystem.
 */
void photos_preseed_images(void) {
    /* 1. Seed /sample.ppm: 32x32 Binary P6 PPM with rich geometric radial gradient */
    if (!vfs_lookup("/sample.ppm")) {
        if (vfs_create("/sample.ppm", VFS_FILE) == 0) {
            int fd = vfs_open("/sample.ppm", 0x02);
            if (fd >= 0) {
                const char* header = "P6\n32 32\n255\n";
                vfs_write(fd, header, (uint32_t)strlen(header));

                uint8_t row[32 * 3];
                for (int y = 0; y < 32; y++) {
                    for (int x = 0; x < 32; x++) {
                        int dx = (x >= 16) ? (x - 16) : (16 - x);
                        int dy = (y >= 16) ? (y - 16) : (16 - y);
                        int r = (x * 255) / 31;
                        int g = (y * 255) / 31;
                        int b = 255 - ((dx + dy) * 255 / 32);
                        if (b < 0) b = 0;
                        if (b > 255) b = 255;

                        row[x * 3 + 0] = (uint8_t)r;
                        row[x * 3 + 1] = (uint8_t)g;
                        row[x * 3 + 2] = (uint8_t)b;
                    }
                    vfs_write(fd, row, 32 * 3);
                }
                vfs_close(fd);
                serial_writestring("[PHOTOS] Created /sample.ppm (32x32 P6 binary)\n");
            }
        }
    }

    /* 2. Seed /avatar.ppm: 32x32 ASCII P3 PPM with portrait silhouette and cyber suit */
    if (!vfs_lookup("/avatar.ppm")) {
        if (vfs_create("/avatar.ppm", VFS_FILE) == 0) {
            int fd = vfs_open("/avatar.ppm", 0x02);
            if (fd >= 0) {
                const char* header = "P3\n# MyOS Mascot Avatar\n32 32\n255\n";
                vfs_write(fd, header, (uint32_t)strlen(header));

                char line_buf[512];
                for (int y = 0; y < 32; y++) {
                    char* p = line_buf;
                    for (int x = 0; x < 32; x++) {
                        /* Background: Dark slate navy */
                        int r = 24, g = 30, b = 42;

                        /* Circular portrait boundary: radius 14 centered at (16, 16) */
                        int cdx = x - 16;
                        int cdy = y - 16;
                        int cd2 = cdx * cdx + cdy * cdy;
                        if (cd2 <= 196) {
                            r = 18; g = 22; b = 32; /* Portrait disc background */
                            if (cd2 >= 169) {
                                r = 50; g = 200; b = 255; /* Cyan rim glow */
                            }
                        }

                        /* Head silhouette: radius 5 centered at (16, 11) */
                        int hdx = x - 16;
                        int hdy = y - 11;
                        int hd2 = hdx * hdx + hdy * hdy;
                        if (hd2 <= 25) {
                            r = 240; g = 180; b = 140; /* Face tone */
                        }
                        /* Tech visor: y=11, x=14..18 */
                        if (y == 11 && x >= 14 && x <= 18) {
                            r = 0; g = 230; b = 255; /* Glowing visor */
                        }

                        /* Shoulders / Torso: y >= 18 */
                        if (y >= 18 && cd2 <= 169) {
                            int sx = x - 16;
                            int sy = y - 24;
                            if ((sx * sx * 100 / 81 + sy * sy * 100 / 36) <= 100) {
                                r = 36; g = 90; b = 180; /* Pilot blue suit */
                                if (x == 16) {
                                    r = 255; g = 200; b = 40; /* Golden zip line */
                                }
                            }
                        }

                        p = append_uint(p, (uint32_t)r);
                        *p++ = ' ';
                        p = append_uint(p, (uint32_t)g);
                        *p++ = ' ';
                        p = append_uint(p, (uint32_t)b);
                        *p++ = ' ';
                    }
                    *p++ = '\n';
                    vfs_write(fd, line_buf, (uint32_t)(p - line_buf));
                }
                vfs_close(fd);
                serial_writestring("[PHOTOS] Created /avatar.ppm (32x32 P3 ASCII)\n");
            }
        }
    }
}

/*
 * ==============================================================================
 * Gallery Scanner & Loader
 * ==============================================================================
 */

/* Scans RamFS root for any existing .ppm image files and adds them to gallery */
static void photos_scan_ramfs(void) {
    vfs_node_t* root = vfs_get_root();
    if (!root || !root->readdir) return;

    for (uint32_t i = 0; i < 32; i++) {
        vfs_dirent_t* d = root->readdir(root, i);
        if (!d) break;
        if (d->type == VFS_DIRECTORY) continue;

        size_t len = strlen(d->name);
        if (len >= 5) {
            /* Case-insensitive check for .ppm extension */
            if (d->name[len - 4] == '.' &&
                (d->name[len - 3] == 'p' || d->name[len - 3] == 'P') &&
                (d->name[len - 2] == 'p' || d->name[len - 2] == 'P') &&
                (d->name[len - 1] == 'm' || d->name[len - 1] == 'M')) {

                /* Avoid duplicate entries */
                bool exists = false;
                for (int g = 0; g < s_gallery_count; g++) {
                    if (strcmp(s_gallery[g].name, d->name) == 0) {
                        exists = true;
                        break;
                    }
                }
                if (!exists && s_gallery_count < PHOTOS_MAX_GALLERY) {
                    char fullpath[VFS_NAME_MAX + 2];
                    fullpath[0] = '/';
                    strncpy(fullpath + 1, d->name, VFS_NAME_MAX);
                    fullpath[VFS_NAME_MAX + 1] = '\0';

                    int w = 0, h = 0;
                    uint32_t* pix = read_ppm_from_vfs(fullpath, &w, &h);
                    if (pix) {
                        strncpy(s_gallery[s_gallery_count].name, d->name, sizeof(s_gallery[0].name) - 1);
                        s_gallery[s_gallery_count].name[sizeof(s_gallery[0].name) - 1] = '\0';
                        s_gallery[s_gallery_count].width = w;
                        s_gallery[s_gallery_count].height = h;
                        s_gallery[s_gallery_count].pixels = pix;
                        s_gallery_count++;
                    }
                }
            }
        }
    }
}

/* Initializes the built-in and filesystem photo gallery */
static void photos_init_gallery(void) {
    if (s_gallery_count > 0) return;

    /* Image 0: Sunset & Mountains */
    int w0 = 0, h0 = 0;
    uint32_t* pix0 = generate_sunset_image(&w0, &h0);
    if (pix0) {
        strcpy(s_gallery[s_gallery_count].name, "Sunset.ppm");
        s_gallery[s_gallery_count].width = w0;
        s_gallery[s_gallery_count].height = h0;
        s_gallery[s_gallery_count].pixels = pix0;
        s_gallery_count++;
    }

    /* Image 1: Retro 8-bit PC Icon */
    int w1 = 0, h1 = 0;
    uint32_t* pix1 = generate_retro_pc_image(&w1, &h1);
    if (pix1) {
        strcpy(s_gallery[s_gallery_count].name, "RetroPC.ppm");
        s_gallery[s_gallery_count].width = w1;
        s_gallery[s_gallery_count].height = h1;
        s_gallery[s_gallery_count].pixels = pix1;
        s_gallery_count++;
    }

    /* Pre-seed sample and avatar files in RamFS if missing */
    photos_preseed_images();

    /* Scan RamFS root for any .ppm files to populate remainder of gallery */
    photos_scan_ramfs();
}

/*
 * ==============================================================================
 * UI Presentation & Rendering
 * ==============================================================================
 */

/* Redraws the bottom status bar with image metadata */
static void photos_update_status(void) {
    if (!s_photos_win) return;

    int sb_y = PHOTOS_STATUS_Y;
    int sb_h = s_photos_win->client_height - sb_y;

    /* Status bar divider */
    window_fill_rect(s_photos_win, 0, sb_y, s_photos_win->client_width, 1, 0x003A4050);
    /* Status bar background */
    window_fill_rect(s_photos_win, 0, sb_y + 1, s_photos_win->client_width, sb_h - 1, 0x001B202A);

    if (s_gallery_count == 0) {
        window_draw_string(s_photos_win, 10, sb_y + 6, "No images found", 0x0090A0B0, 0x001B202A, false);
        return;
    }

    photo_entry_t* cur = &s_gallery[s_current_index];
    char status_str[128];
    char num_buf[16];

    /* Format: "[1/4] Sunset.ppm | 96x64 px | 1x | RGB 24-bit" */
    status_str[0] = '[';
    int_to_str(s_current_index + 1, num_buf);
    strcpy(status_str + 1, num_buf);
    strcat(status_str, "/");
    int_to_str(s_gallery_count, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, "] ");
    strcat(status_str, cur->name);
    strcat(status_str, " | ");

    int_to_str(cur->width, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, "x");
    int_to_str(cur->height, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, " px | ");

    int_to_str(s_zoom, num_buf);
    strcat(status_str, num_buf);
    strcat(status_str, "x | ");

    if (s_filter_invert && s_filter_grayscale) {
        strcat(status_str, "Gray+Invert");
    } else if (s_filter_invert) {
        strcat(status_str, "Inverted");
    } else if (s_filter_grayscale) {
        strcat(status_str, "Grayscale");
    } else {
        strcat(status_str, "RGB 24-bit");
    }

    window_draw_string(s_photos_win, 8, sb_y + 6, status_str, 0x00D0D8E8, 0x001B202A, false);
}

/* Redraws the central viewport canvas and re-renders active image */
static void photos_redraw_canvas(void) {
    if (!s_photos_win) return;

    int vp_x = PHOTOS_VP_X;
    int vp_y = PHOTOS_VP_Y;
    int vp_w = PHOTOS_VP_W;
    int vp_h = PHOTOS_VP_H;

    /* 1. Viewport Outer Border and Dark Backdrop */
    window_draw_rect(s_photos_win, vp_x, vp_y, vp_w, vp_h, 0x002B303C);
    window_fill_rect(s_photos_win, vp_x + 1, vp_y + 1, vp_w - 2, vp_h - 2, 0x0012141A);

    /* 2. Render Active Image */
    if (s_gallery_count > 0) {
        photo_entry_t* img = &s_gallery[s_current_index];
        int dw = img->width * s_zoom;
        int dh = img->height * s_zoom;

        /* Calculate centered origin inside the viewport */
        int img_x = vp_x + 1 + (vp_w - 2 - dw) / 2;
        int img_y = vp_y + 1 + (vp_h - 2 - dh) / 2;

        /* Viewport clipping boundaries */
        int v_left   = vp_x + 1;
        int v_top    = vp_y + 1;
        int v_right  = vp_x + vp_w - 1;
        int v_bottom = vp_y + vp_h - 1;

        for (int py = 0; py < img->height; py++) {
            int by = img_y + py * s_zoom;
            if (by + s_zoom <= v_top || by >= v_bottom) continue;

            for (int px = 0; px < img->width; px++) {
                int bx = img_x + px * s_zoom;
                if (bx + s_zoom <= v_left || bx >= v_right) continue;

                uint32_t col = img->pixels[py * img->width + px];
                uint32_t r = (col >> 16) & 0xFF;
                uint32_t g = (col >> 8) & 0xFF;
                uint32_t b = col & 0xFF;

                /* Apply Grayscale Luminance filter: Y = (77*R + 150*G + 29*B) >> 8 */
                if (s_filter_grayscale) {
                    uint32_t lum = (r * 77 + g * 150 + b * 29) >> 8;
                    r = lum;
                    g = lum;
                    b = lum;
                }

                /* Apply Invert filter: 255 - component */
                if (s_filter_invert) {
                    r = 255 - r;
                    g = 255 - g;
                    b = 255 - b;
                }

                col = (r << 16) | (g << 8) | b;

                /* Clip block boundaries to strictly prevent viewport overflow */
                int cbx = bx;
                int cby = by;
                int cbw = s_zoom;
                int cbh = s_zoom;

                if (cbx < v_left) { cbw -= (v_left - cbx); cbx = v_left; }
                if (cby < v_top) { cbh -= (v_top - cby); cby = v_top; }
                if (cbx + cbw > v_right) cbw = v_right - cbx;
                if (cby + cbh > v_bottom) cbh = v_bottom - cby;

                if (cbw > 0 && cbh > 0) {
                    if (cbw == 1 && cbh == 1) {
                        window_putpixel(s_photos_win, cbx, cby, col);
                    } else {
                        window_fill_rect(s_photos_win, cbx, cby, cbw, cbh, col);
                    }
                }
            }
        }
    } else {
        window_draw_string(s_photos_win, vp_x + 130, vp_y + 110, "No Images Available", 0x00808A9C, 0x0012141A, true);
    }

    /* 3. Redraw Status Bar */
    photos_update_status();

    /* 4. Trigger window compositing */
    wm_invalidate_rect(s_photos_win->x, s_photos_win->y, s_photos_win->width, s_photos_win->height);
}

/*
 * ==============================================================================
 * Toolbar Button Callbacks
 * ==============================================================================
 */

static void on_btn_prev(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    if (s_gallery_count > 0) {
        s_current_index = (s_current_index - 1 + s_gallery_count) % s_gallery_count;
        photos_redraw_canvas();
    }
}

static void on_btn_next(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    if (s_gallery_count > 0) {
        s_current_index = (s_current_index + 1) % s_gallery_count;
        photos_redraw_canvas();
    }
}

static void on_btn_zoom_in(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    if (s_zoom < PHOTOS_MAX_ZOOM) {
        s_zoom++;
        photos_redraw_canvas();
    }
}

static void on_btn_zoom_out(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    if (s_zoom > PHOTOS_MIN_ZOOM) {
        s_zoom--;
        photos_redraw_canvas();
    }
}

static void on_btn_invert(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    s_filter_invert = !s_filter_invert;
    photos_redraw_canvas();
}

static void on_btn_grayscale(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    s_filter_grayscale = !s_filter_grayscale;
    photos_redraw_canvas();
}

/*
 * ==============================================================================
 * Window Event Handlers
 * ==============================================================================
 */

/* Handles keyboard shortcuts for quick image manipulation */
static void on_photos_key(window_t* win, char c) {
    (void)win;
    if (c == 'n' || c == 'N' || c == ' ' || c == 0x12) {
        /* Next image: 'N', Space, or Right Arrow */
        if (s_gallery_count > 0) {
            s_current_index = (s_current_index + 1) % s_gallery_count;
            photos_redraw_canvas();
        }
    } else if (c == 'p' || c == 'P' || c == 0x08 || c == 0x11) {
        /* Previous image: 'P', Backspace, or Left Arrow */
        if (s_gallery_count > 0) {
            s_current_index = (s_current_index - 1 + s_gallery_count) % s_gallery_count;
            photos_redraw_canvas();
        }
    } else if (c == '+' || c == '=') {
        /* Zoom in: '+' or '=' */
        if (s_zoom < PHOTOS_MAX_ZOOM) {
            s_zoom++;
            photos_redraw_canvas();
        }
    } else if (c == '-' || c == '_') {
        /* Zoom out: '-' or '_' */
        if (s_zoom > PHOTOS_MIN_ZOOM) {
            s_zoom--;
            photos_redraw_canvas();
        }
    } else if (c == 'i' || c == 'I') {
        /* Invert color toggle */
        s_filter_invert = !s_filter_invert;
        photos_redraw_canvas();
    } else if (c == 'g' || c == 'G') {
        /* Grayscale color toggle */
        s_filter_grayscale = !s_filter_grayscale;
        photos_redraw_canvas();
    }
}

/* Cleans up application window handle when closed */
static void on_photos_close(window_t* win) {
    (void)win;
    wm_remove_window(s_photos_win);
    s_photos_win = NULL;
}

/*
 * ==============================================================================
 * Public Factory Functions
 * ==============================================================================
 */

/*
 * photos_app_create:
 * Creates and initializes the Photos application window at screen position (x, y).
 */
window_t* photos_app_create(int32_t x, int32_t y) {
    if (s_photos_win) {
        wm_bring_to_front(s_photos_win);
        return s_photos_win;
    }

    /* Clamp initial window coordinates to visible screen region */
    if (x < 0 || x > 600) x = 140;
    if (y < 0 || y > 400) y = 100;

    /* Initialize gallery and discover RamFS images */
    photos_init_gallery();

    /* Allocate desktop window object */
    s_photos_win = window_create((int)x, (int)y, PHOTOS_WIN_WIDTH, PHOTOS_WIN_HEIGHT,
                                 "Photos - Image Viewer",
                                 WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_photos_win) return NULL;

    s_photos_win->on_close = on_photos_close;
    s_photos_win->on_key   = on_photos_key;

    /* Clear window client canvas to dark toolbar slate */
    window_clear_client(s_photos_win, 0x00242832);

    /* Toolbar bottom separator */
    window_fill_rect(s_photos_win, 0, 37, s_photos_win->client_width, 1, 0x003A4050);

    /* Attach Toolbar Push Buttons: [ Prev ], [ Next ], [ Zoom+ ], [ Zoom- ], [ Invert ], [ Grayscale ] */
    widget_add_to_window(s_photos_win, widget_create_button(8,   6, 54, 25, "Prev",      on_btn_prev,      NULL));
    widget_add_to_window(s_photos_win, widget_create_button(68,  6, 54, 25, "Next",      on_btn_next,      NULL));
    widget_add_to_window(s_photos_win, widget_create_button(128, 6, 58, 25, "Zoom+",     on_btn_zoom_in,   NULL));
    widget_add_to_window(s_photos_win, widget_create_button(192, 6, 58, 25, "Zoom-",     on_btn_zoom_out,  NULL));
    widget_add_to_window(s_photos_win, widget_create_button(256, 6, 66, 25, "Invert",    on_btn_invert,    NULL));
    widget_add_to_window(s_photos_win, widget_create_button(328, 6, 80, 25, "Grayscale", on_btn_grayscale, NULL));

    /* Pre-render toolbar widgets into client surface */
    widget_render_all(s_photos_win);

    /* Render central image canvas and status bar */
    photos_redraw_canvas();

    /* Register window with Window Manager */
    wm_add_window(s_photos_win);
    serial_writestring("[PHOTOS] Photos - Image Viewer application launched.\n");

    return s_photos_win;
}

/*
 * app_photos_open:
 * Convenience launcher for desktop shortcuts and start menu.
 */
void app_photos_open(void) {
    photos_app_create(140, 100);
}
