#include "apps/breakout.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/theme.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/mouse.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * breakout.c:
 * Retro 8-Bit Breakout / Brick Breaker Arcade Game for MyOS.
 *
 * Why this is needed:
 * Provides an authentic retro arcade desktop gaming experience on MyOS.
 * It demonstrates:
 * 1. High-precision fixed-point sub-pixel 2D physics.
 * 2. Multi-segmented paddle deflection where rebound angle varies across the paddle.
 * 3. 4-tier colored brick grid destruction with distinct point awards.
 * 4. Comprehensive game state machine (READY, PLAYING, PAUSED, GAME OVER, VICTORY).
 * 5. Dual responsive input scheme supporting keyboard (Arrows/WASD) and mouse steering.
 */

/* Window Geometry */
#define BREAKOUT_WIN_W         380
#define BREAKOUT_WIN_H         380

/* Arena (Playing Field) Geometry inside Client Area (376 x 354) */
#define ARENA_W                340
#define ARENA_H                280
#define ARENA_X                ((376 - ARENA_W) / 2) /* 18 px centered */
#define ARENA_Y                38                    /* Beneath top HUD */

/* HUD Top Bar Layout */
#define HUD_X                  ARENA_X
#define HUD_Y                  8
#define HUD_W                  ARENA_W
#define HUD_H                  24

/* Bricks Layout */
#define BREAKOUT_ROWS          4
#define BREAKOUT_COLS          8
#define BREAKOUT_TOTAL_BRICKS  (BREAKOUT_ROWS * BREAKOUT_COLS) /* 32 bricks */
#define BRICK_W                38
#define BRICK_H                12
#define BRICK_GAP_X            4
#define BRICK_GAP_Y            4
#define BRICK_START_X          (ARENA_X + 4)         /* 22 */
#define BRICK_START_Y          (ARENA_Y + 24)        /* 62 */

/* Paddle Parameters */
#define PADDLE_W               60
#define PADDLE_H               10
#define PADDLE_Y               (ARENA_Y + ARENA_H - 22) /* 296 */
#define PADDLE_MIN_X           (ARENA_X + 2)            /* 20 */
#define PADDLE_MAX_X           (ARENA_X + ARENA_W - 2 - PADDLE_W) /* 296 */
#define PADDLE_STEP_KB         20                       /* Pixels per key press */

/* Ball Parameters */
#define BALL_SIZE              6

/* Fixed-Point Arithmetic Definitions (8-bit fractional precision: 1 px = 256 units) */
#define FP_SHIFT               8
#define INT_TO_FP(v)           ((int32_t)(v) << FP_SHIFT)
#define FP_TO_INT(v)           ((int32_t)((v) >> FP_SHIFT))

/* Game Lifecycle States */
typedef enum {
    STATE_READY = 0,    /* Ball rested on paddle, waiting for serve */
    STATE_PLAYING,      /* Ball bouncing, active gameplay */
    STATE_PAUSED,       /* Game suspended */
    STATE_GAME_OVER,    /* All lives depleted, final score shown */
    STATE_VICTORY       /* All bricks cleared, celebratory banner */
} breakout_state_t;

/* Brick Data Structure */
typedef struct {
    int      x;
    int      y;
    int      w;
    int      h;
    int      points;
    uint32_t color_base;
    uint32_t color_hi;
    uint32_t color_sh;
    bool     alive;
} brick_t;

/* Static Game State Variables */
static window_t*         s_breakout_win = NULL;
static breakout_state_t  s_state = STATE_READY;

static int               s_score = 0;
static int               s_high_score = 0;
static int               s_lives = 3;
static int               s_level = 1;
static int               s_bricks_remaining = BREAKOUT_TOTAL_BRICKS;

static brick_t           s_bricks[BREAKOUT_ROWS][BREAKOUT_COLS];

/* Paddle Position (X relative to window client) */
static int               s_paddle_x = ARENA_X + (ARENA_W - PADDLE_W) / 2;

/* Ball Fixed-Point Coordinates and Velocities */
static int32_t           s_ball_x_fp = 0;
static int32_t           s_ball_y_fp = 0;
static int32_t           s_ball_vx_fp = 0;
static int32_t           s_ball_vy_fp = 0;

/* Tick rate-limiting */
static uint32_t          s_last_tick = 0;
static int               s_last_mouse_x = -1;

/* Forward Declarations */
static void breakout_redraw_all(void);
static void breakout_init_level(int level);
static void breakout_reset_game(void);

/*
 * fmt_4digit:
 * Formats a non-negative integer into a 4-digit zero-padded decimal string.
 */
static void fmt_4digit(int val, char* buf) {
    if (val < 0) val = 0;
    if (val > 9999) val = 9999;
    buf[0] = (char)('0' + ((val / 1000) % 10));
    buf[1] = (char)('0' + ((val / 100) % 10));
    buf[2] = (char)('0' + ((val / 10) % 10));
    buf[3] = (char)('0' + (val % 10));
    buf[4] = '\0';
}

/*
 * fmt_int:
 * Converts an integer into standard ASCII decimal string representation.
 */
static void fmt_int(int val, char* buf) {
    if (val <= 0) {
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

/*
 * draw_heart:
 * Renders an authentic 7x7 pixel heart glyph into the window client surface.
 * Used for arcade HUD lives indicator.
 */
static void draw_heart(window_t* win, int x, int y, uint32_t color) {
    /* Row 0: . X X . X X . */
    window_fill_rect(win, x + 1, y + 0, 2, 1, color);
    window_fill_rect(win, x + 4, y + 0, 2, 1, color);
    /* Row 1: X X X X X X X */
    window_fill_rect(win, x + 0, y + 1, 7, 1, color);
    /* Row 2: X X X X X X X */
    window_fill_rect(win, x + 0, y + 2, 7, 1, color);
    /* Row 3: . X X X X X . */
    window_fill_rect(win, x + 1, y + 3, 5, 1, color);
    /* Row 4: . . X X X . . */
    window_fill_rect(win, x + 2, y + 4, 3, 1, color);
    /* Row 5: . . . X . . . */
    window_fill_rect(win, x + 3, y + 5, 1, 1, color);
}

/*
 * breakout_place_ball_on_paddle:
 * Centers the ball atop the paddle in preparation for serve.
 */
static void breakout_place_ball_on_paddle(void) {
    int bx = s_paddle_x + (PADDLE_W - BALL_SIZE) / 2;
    int by = PADDLE_Y - BALL_SIZE - 1;
    s_ball_x_fp = INT_TO_FP(bx);
    s_ball_y_fp = INT_TO_FP(by);
    /* Initial serve velocity: upward with slight horizontal angle */
    s_ball_vx_fp = (timer_get_ticks() & 1) ? 280 : -280;
    s_ball_vy_fp = -850;
}

/*
 * breakout_init_level:
 * Populates brick positions, 4-tier color gradients, point values, and resets ball.
 */
static void breakout_init_level(int level) {
    (void)level;
    s_bricks_remaining = BREAKOUT_TOTAL_BRICKS;

    for (int r = 0; r < BREAKOUT_ROWS; r++) {
        for (int c = 0; c < BREAKOUT_COLS; c++) {
            brick_t* b = &s_bricks[r][c];
            b->x = BRICK_START_X + c * (BRICK_W + BRICK_GAP_X);
            b->y = BRICK_START_Y + r * (BRICK_H + BRICK_GAP_Y);
            b->w = BRICK_W;
            b->h = BRICK_H;
            b->alive = true;

            /* Set tier colors and point awards:
             * Row 0 (Top): Red (7 pts)
             * Row 1: Orange (5 pts)
             * Row 2: Green (3 pts)
             * Row 3: Yellow (1 pt)
             */
            switch (r) {
                case 0: /* Red Brick */
                    b->points     = 7;
                    b->color_base = 0x00EF4444; /* Tailwind Red 500 */
                    b->color_hi   = 0x00F87171; /* Highlight */
                    b->color_sh   = 0x00B91C1C; /* Shadow */
                    break;
                case 1: /* Orange Brick */
                    b->points     = 5;
                    b->color_base = 0x00F97316; /* Orange 500 */
                    b->color_hi   = 0x00FB923C;
                    b->color_sh   = 0x00C2410C;
                    break;
                case 2: /* Green Brick */
                    b->points     = 3;
                    b->color_base = 0x0022C55E; /* Green 500 */
                    b->color_hi   = 0x004ADE80;
                    b->color_sh   = 0x0015803D;
                    break;
                case 3: /* Yellow Brick */
                default:
                    b->points     = 1;
                    b->color_base = 0x00EAB308; /* Yellow 500 */
                    b->color_hi   = 0x00FACC15;
                    b->color_sh   = 0x00A16207;
                    break;
            }
        }
    }

    s_state = STATE_READY;
    breakout_place_ball_on_paddle();
}

/*
 * breakout_reset_game:
 * Resets score, lives, level, and brick field to initial game start state.
 */
static void breakout_reset_game(void) {
    s_score = 0;
    s_lives = 3;
    s_level = 1;
    s_paddle_x = ARENA_X + (ARENA_W - PADDLE_W) / 2;
    breakout_init_level(s_level);
}

/*
 * breakout_draw_hud:
 * Renders top status HUD: Score: 0000 | High Score: 0000 | Lives: [hearts] | Level: 1.
 */
static void breakout_draw_hud(void) {
    if (!s_breakout_win) return;

    /* 1. HUD Background Bar */
    window_fill_rect(s_breakout_win, HUD_X, HUD_Y, HUD_W, HUD_H, 0x00161B26);
    window_draw_rect(s_breakout_win, HUD_X, HUD_Y, HUD_W, HUD_H, 0x002E384D);

    /* 2. Format Score String */
    char score_digits[8];
    fmt_4digit(s_score, score_digits);
    char score_str[24] = "Score: ";
    strlcat(score_str, score_digits, sizeof(score_str));
    window_draw_string(s_breakout_win, HUD_X + 8, HUD_Y + 4, score_str, 0x00F8FAFC, 0, true);

    /* 3. Format High Score String */
    char high_digits[8];
    fmt_4digit(s_high_score, high_digits);
    char high_str[24] = "High: ";
    strlcat(high_str, high_digits, sizeof(high_str));
    window_draw_string(s_breakout_win, HUD_X + 104, HUD_Y + 4, high_str, 0x00FACC15, 0, true);

    /* 4. Lives Indicator with graphical 7x7 hearts */
    window_draw_string(s_breakout_win, HUD_X + 196, HUD_Y + 4, "Lives:", 0x0094A3B8, 0, true);
    for (int i = 0; i < 3; i++) {
        uint32_t heart_col = (i < s_lives) ? 0x00EF4444 : 0x00334155;
        draw_heart(s_breakout_win, HUD_X + 248 + i * 11, HUD_Y + 8, heart_col);
    }

    /* 5. Format Level String */
    char lvl_digit[8];
    fmt_int(s_level, lvl_digit);
    char lvl_str[16] = "Lvl:";
    strlcat(lvl_str, lvl_digit, sizeof(lvl_str));
    window_draw_string(s_breakout_win, HUD_X + 292, HUD_Y + 4, lvl_str, 0x0038BDF8, 0, true);
}

/*
 * breakout_draw_arena:
 * Renders the playing field, border, active bricks, paddle, and bouncing ball.
 */
static void breakout_draw_arena(void) {
    if (!s_breakout_win) return;

    /* 1. Arena Black Surface */
    window_fill_rect(s_breakout_win, ARENA_X, ARENA_Y, ARENA_W, ARENA_H, 0x000A0E17);

    /* 2. Neon Dual-Line Frame (Cyan outer, deep blue inner) */
    window_draw_rect(s_breakout_win, ARENA_X, ARENA_Y, ARENA_W, ARENA_H, 0x0038BDF8);
    window_draw_rect(s_breakout_win, ARENA_X + 1, ARENA_Y + 1, ARENA_W - 2, ARENA_H - 2, 0x001E3A8A);

    /* 3. Bricks Grid */
    for (int r = 0; r < BREAKOUT_ROWS; r++) {
        for (int c = 0; c < BREAKOUT_COLS; c++) {
            brick_t* b = &s_bricks[r][c];
            if (!b->alive) continue;

            /* Base fill */
            window_fill_rect(s_breakout_win, b->x, b->y, b->w, b->h, b->color_base);

            /* 3D Bevel: top & left highlight */
            window_fill_rect(s_breakout_win, b->x, b->y, b->w, 1, b->color_hi);
            window_fill_rect(s_breakout_win, b->x, b->y, 1, b->h, b->color_hi);

            /* 3D Bevel: bottom & right shadow */
            window_fill_rect(s_breakout_win, b->x, b->y + b->h - 1, b->w, 1, b->color_sh);
            window_fill_rect(s_breakout_win, b->x + b->w - 1, b->y, 1, b->h, b->color_sh);
        }
    }

    /* 4. Player Paddle */
    /* Metallic arcade body with cyan gradient highlight and center target line */
    window_fill_rect(s_breakout_win, s_paddle_x, PADDLE_Y, PADDLE_W, PADDLE_H, 0x000284C7);
    window_fill_rect(s_breakout_win, s_paddle_x, PADDLE_Y, PADDLE_W, 2, 0x007DD3FC); /* Highlight */
    window_fill_rect(s_breakout_win, s_paddle_x, PADDLE_Y + PADDLE_H - 2, PADDLE_W, 2, 0x000369A1); /* Shadow */
    /* Center target stripe */
    window_fill_rect(s_breakout_win, s_paddle_x + PADDLE_W / 2 - 2, PADDLE_Y + 2, 4, PADDLE_H - 4, 0x00FFFFFF);

    /* 5. Bouncing Ball (6x6 px with soft corners) */
    int bx = FP_TO_INT(s_ball_x_fp);
    int by = FP_TO_INT(s_ball_y_fp);
    window_fill_rect(s_breakout_win, bx, by + 1, BALL_SIZE, BALL_SIZE - 2, 0x00FFFFFF);
    window_fill_rect(s_breakout_win, bx + 1, by, BALL_SIZE - 2, BALL_SIZE, 0x00FFFFFF);
    /* Soft center reflection */
    window_putpixel(s_breakout_win, bx + 2, by + 2, 0x00E0F2FE);

    /* 6. State Overlays */
    if (s_state == STATE_READY) {
        int bw = 248, bh = 46;
        int ox = ARENA_X + (ARENA_W - bw) / 2;
        int oy = ARENA_Y + 124;
        window_fill_rect(s_breakout_win, ox, oy, bw, bh, 0x000F172A);
        window_draw_rect(s_breakout_win, ox, oy, bw, bh, 0x0038BDF8);
        window_draw_string(s_breakout_win, ox + 80, oy + 6, "- READY -", 0x00FACC15, 0, true);
        window_draw_string(s_breakout_win, ox + 8, oy + 24, "Press SPACE or Click to Serve", 0x00E2E8F0, 0, true);
    } else if (s_state == STATE_GAME_OVER) {
        int bw = 248, bh = 68;
        int ox = ARENA_X + (ARENA_W - bw) / 2;
        int oy = ARENA_Y + 114;
        window_fill_rect(s_breakout_win, ox, oy, bw, bh, 0x000F172A);
        window_draw_rect(s_breakout_win, ox, oy, bw, bh, 0x00EF4444);
        window_draw_string(s_breakout_win, ox + 80, oy + 8, "GAME OVER", 0x00EF4444, 0, true);

        char final_score_str[32] = "Final Score: ";
        char sc_buf[16];
        fmt_int(s_score, sc_buf);
        strlcat(final_score_str, sc_buf, sizeof(final_score_str));
        window_draw_string(s_breakout_win, ox + 52, oy + 28, final_score_str, 0x00F8FAFC, 0, true);
        window_draw_string(s_breakout_win, ox + 28, oy + 48, "Press SPACE to Restart", 0x0094A3B8, 0, true);
    } else if (s_state == STATE_VICTORY) {
        int bw = 256, bh = 68;
        int ox = ARENA_X + (ARENA_W - bw) / 2;
        int oy = ARENA_Y + 114;
        window_fill_rect(s_breakout_win, ox, oy, bw, bh, 0x000F172A);
        window_draw_rect(s_breakout_win, ox, oy, bw, bh, 0x0022C55E);
        window_draw_string(s_breakout_win, ox + 84, oy + 8, "VICTORY!", 0x0022C55E, 0, true);
        window_draw_string(s_breakout_win, ox + 32, oy + 28, "All Bricks Destroyed!", 0x00FACC15, 0, true);
        window_draw_string(s_breakout_win, ox + 24, oy + 48, "Press SPACE for Next Lvl", 0x00F8FAFC, 0, true);
    } else if (s_state == STATE_PAUSED) {
        int bw = 180, bh = 36;
        int ox = ARENA_X + (ARENA_W - bw) / 2;
        int oy = ARENA_Y + 128;
        window_fill_rect(s_breakout_win, ox, oy, bw, bh, 0x001E293B);
        window_draw_rect(s_breakout_win, ox, oy, bw, bh, 0x00FACC15);
        window_draw_string(s_breakout_win, ox + 40, oy + 10, "** PAUSED **", 0x00FACC15, 0, true);
    }

    /* 7. Bottom Helper Text */
    const char* hint_text;
    if (s_state == STATE_READY) {
        hint_text = "[SPACE] Serve  |  [A/D or Mouse] Steer";
    } else if (s_state == STATE_PLAYING) {
        hint_text = "[A/D or Mouse] Move  |  [P] Pause";
    } else if (s_state == STATE_PAUSED) {
        hint_text = "Game Paused - Press [P] to Resume";
    } else if (s_state == STATE_GAME_OVER) {
        hint_text = "[SPACE] or Click: Play Again";
    } else {
        hint_text = "[SPACE] or Click: Continue Next Lvl";
    }

    int hint_len = (int)strlen(hint_text);
    int hint_x = (s_breakout_win->client_width - (hint_len * FONT_GLYPH_WIDTH)) / 2;
    window_draw_string(s_breakout_win, hint_x, 328, hint_text, 0x0064748B, 0, true);
}

/*
 * breakout_redraw_all:
 * Clears client canvas, paints HUD, arena, widgets, and flushes dirty region to screen.
 */
static void breakout_redraw_all(void) {
    if (!s_breakout_win) return;

    /* Base Window Background */
    const theme_t* th = theme_get_current();
    uint32_t bg_col = th ? th->bg_window_client : 0x00141824;
    window_clear_client(s_breakout_win, bg_col);

    /* Render HUD and Arena elements */
    breakout_draw_hud();
    breakout_draw_arena();

    /* Flush compositor */
    wm_invalidate_rect(s_breakout_win->x, s_breakout_win->y,
                       s_breakout_win->width, s_breakout_win->height);
}

/*
 * on_breakout_key:
 * Window Manager character callback.
 */
static void on_breakout_key(window_t* win, char c) {
    (void)win;
    breakout_handle_key((uint8_t)c);
}

/*
 * on_breakout_click:
 * Window Manager mouse click callback inside client canvas.
 */
static void on_breakout_click(window_t* win, int rx, int ry) {
    (void)win;
    (void)rx;
    (void)ry;

    if (s_state == STATE_READY) {
        /* Click to serve */
        s_state = STATE_PLAYING;
        s_ball_vx_fp = (timer_get_ticks() & 1) ? 280 : -280;
        s_ball_vy_fp = -850;
        breakout_redraw_all();
    } else if (s_state == STATE_GAME_OVER) {
        /* Click to restart */
        breakout_reset_game();
        breakout_redraw_all();
    } else if (s_state == STATE_VICTORY) {
        /* Click to advance level */
        s_level++;
        breakout_init_level(s_level);
        breakout_redraw_all();
    } else if (s_state == STATE_PAUSED) {
        /* Click to unpause */
        s_state = STATE_PLAYING;
        breakout_redraw_all();
    }
}

/*
 * on_breakout_close:
 * Window Manager window close callback.
 */
static void on_breakout_close(window_t* win) {
    wm_remove_window(win);
    s_breakout_win = NULL;
    serial_writestring("[BREAKOUT] Window closed\n");
}

/*
 * breakout_handle_key:
 * Dispatches key inputs (supporting both raw scancodes and ASCII characters).
 */
void breakout_handle_key(uint8_t code) {
    /* 1. Left Movement (Left Arrow 0x4B, 'a', 'A', or 'A' scancode 0x1E) */
    if (code == 0x4B || code == 'a' || code == 'A' || code == 0x1E) {
        s_paddle_x -= PADDLE_STEP_KB;
        if (s_paddle_x < PADDLE_MIN_X) {
            s_paddle_x = PADDLE_MIN_X;
        }
        if (s_state == STATE_READY) {
            breakout_place_ball_on_paddle();
        }
        breakout_redraw_all();
        return;
    }

    /* 2. Right Movement (Right Arrow 0x4D, 'd', 'D', or 'D' scancode 0x20) */
    if (code == 0x4D || code == 'd' || code == 'D' || code == 0x20) {
        s_paddle_x += PADDLE_STEP_KB;
        if (s_paddle_x > PADDLE_MAX_X) {
            s_paddle_x = PADDLE_MAX_X;
        }
        if (s_state == STATE_READY) {
            breakout_place_ball_on_paddle();
        }
        breakout_redraw_all();
        return;
    }

    /* 3. Action / Serve / Restart (Space 0x39 or ' ', Enter 0x1C or '\n') */
    if (code == 0x39 || code == ' ' || code == '\n' || code == 0x1C) {
        if (s_state == STATE_READY) {
            s_state = STATE_PLAYING;
            s_ball_vx_fp = (timer_get_ticks() & 1) ? 280 : -280;
            s_ball_vy_fp = -850;
            breakout_redraw_all();
        } else if (s_state == STATE_GAME_OVER) {
            breakout_reset_game();
            breakout_redraw_all();
        } else if (s_state == STATE_VICTORY) {
            s_level++;
            breakout_init_level(s_level);
            breakout_redraw_all();
        }
        return;
    }

    /* 4. Pause Toggle ('p', 'P', or scancode 0x19) */
    if (code == 'p' || code == 'P' || code == 0x19) {
        if (s_state == STATE_PLAYING) {
            s_state = STATE_PAUSED;
            breakout_redraw_all();
        } else if (s_state == STATE_PAUSED) {
            s_state = STATE_PLAYING;
            breakout_redraw_all();
        }
        return;
    }
}

/*
 * breakout_tick:
 * Periodic game update called from the desktop main event loop.
 * Updates smooth mouse paddle tracking, fixed-point ball physics,
 * wall bounces, paddle deflection, brick collision detection, and redrawing.
 */
void breakout_tick(void) {
    if (!s_breakout_win) return;
    if (!(s_breakout_win->flags & WINDOW_FLAG_VISIBLE)) return;
    if (s_breakout_win->flags & WINDOW_FLAG_MINIMIZED) return;

    /* A. Smooth Mouse Paddle Tracking */
    int mx = 0, my = 0;
    mouse_get_position(&mx, &my);

    /* If mouse has moved horizontally and is within active window coordinates */
    if (mx != s_last_mouse_x) {
        s_last_mouse_x = mx;
        int client_rel_x = mx - s_breakout_win->client_x;
        int client_rel_y = my - s_breakout_win->client_y;

        /* Track paddle when mouse is within the client window's bounds */
        if (client_rel_y >= 0 && client_rel_y < s_breakout_win->client_height &&
            client_rel_x >= 0 && client_rel_x < s_breakout_win->client_width) {
            int target_x = client_rel_x - (PADDLE_W / 2);
            if (target_x < PADDLE_MIN_X) target_x = PADDLE_MIN_X;
            if (target_x > PADDLE_MAX_X) target_x = PADDLE_MAX_X;

            if (target_x != s_paddle_x) {
                s_paddle_x = target_x;
                if (s_state == STATE_READY) {
                    breakout_place_ball_on_paddle();
                }
                breakout_redraw_all();
            }
        }
    }

    /* B. Rate-limit physics updates to 50 Hz (every 2 PIT ticks @ 100 Hz timer) */
    uint32_t now = timer_get_ticks();
    if (now - s_last_tick < 2) {
        return;
    }
    s_last_tick = now;

    /* Ball moves only in active PLAYING state */
    if (s_state != STATE_PLAYING) {
        return;
    }

    /* 1. Advance Ball Position via Fixed-Point Velocities */
    s_ball_x_fp += s_ball_vx_fp;
    s_ball_y_fp += s_ball_vy_fp;

    int bx = FP_TO_INT(s_ball_x_fp);
    int by = FP_TO_INT(s_ball_y_fp);

    /* 2. Wall Collisions: Left, Right, and Top */
    /* Left Wall Bounce */
    if (bx < ARENA_X + 2) {
        bx = ARENA_X + 2;
        s_ball_x_fp = INT_TO_FP(bx);
        if (s_ball_vx_fp < 0) s_ball_vx_fp = -s_ball_vx_fp;
    }
    /* Right Wall Bounce */
    if (bx + BALL_SIZE > ARENA_X + ARENA_W - 2) {
        bx = ARENA_X + ARENA_W - 2 - BALL_SIZE;
        s_ball_x_fp = INT_TO_FP(bx);
        if (s_ball_vx_fp > 0) s_ball_vx_fp = -s_ball_vx_fp;
    }
    /* Top Wall Bounce */
    if (by < ARENA_Y + 2) {
        by = ARENA_Y + 2;
        s_ball_y_fp = INT_TO_FP(by);
        if (s_ball_vy_fp < 0) s_ball_vy_fp = -s_ball_vy_fp;
    }

    /* 3. Bottom Miss: Ball lost beneath paddle */
    if (by > ARENA_Y + ARENA_H) {
        s_lives--;
        if (s_lives > 0) {
            s_state = STATE_READY;
            breakout_place_ball_on_paddle();
        } else {
            s_state = STATE_GAME_OVER;
            if (s_score > s_high_score) {
                s_high_score = s_score;
            }
        }
        breakout_redraw_all();
        return;
    }

    /* 4. Paddle Collision with Variable Rebound Angle */
    if (s_ball_vy_fp > 0 &&
        by + BALL_SIZE >= PADDLE_Y && by <= PADDLE_Y + PADDLE_H &&
        bx + BALL_SIZE >= s_paddle_x && bx <= s_paddle_x + PADDLE_W) {

        /* Rest ball on paddle top surface */
        by = PADDLE_Y - BALL_SIZE;
        s_ball_y_fp = INT_TO_FP(by);

        /* Calculate hit offset from paddle center */
        int paddle_center = s_paddle_x + PADDLE_W / 2;
        int ball_center   = bx + BALL_SIZE / 2;
        int diff          = ball_center - paddle_center; /* -30 to +30 */

        /* Segmented deflection angles:
         * Center: steep vertical bounce, low horizontal velocity.
         * Inner edges: moderate angle.
         * Extreme corners: sharp acute slice angle.
         */
        if (diff < -22) {
            s_ball_vx_fp = -1024; /* ~4.0 px/tick sharp left */
            s_ball_vy_fp = -650;  /* ~2.5 px/tick up */
        } else if (diff < -12) {
            s_ball_vx_fp = -768;  /* ~3.0 px/tick left */
            s_ball_vy_fp = -750;  /* ~2.9 px/tick up */
        } else if (diff < -4) {
            s_ball_vx_fp = -460;  /* ~1.8 px/tick shallow left */
            s_ball_vy_fp = -850;  /* ~3.3 px/tick up */
        } else if (diff <= 4) {
            /* Center hit bounces nearly straight up */
            s_ball_vx_fp = (s_ball_vx_fp < 0) ? -220 : 220;
            s_ball_vy_fp = -920;  /* ~3.6 px/tick steep up */
        } else if (diff <= 12) {
            s_ball_vx_fp = 460;   /* ~1.8 px/tick shallow right */
            s_ball_vy_fp = -850;
        } else if (diff <= 22) {
            s_ball_vx_fp = 768;   /* ~3.0 px/tick right */
            s_ball_vy_fp = -750;
        } else {
            s_ball_vx_fp = 1024;  /* ~4.0 px/tick sharp right */
            s_ball_vy_fp = -650;
        }
    }

    /* 5. Brick Collisions */
    for (int r = 0; r < BREAKOUT_ROWS; r++) {
        for (int c = 0; c < BREAKOUT_COLS; c++) {
            brick_t* b = &s_bricks[r][c];
            if (!b->alive) continue;

            /* AABB overlap test */
            if (bx + BALL_SIZE >= b->x && bx <= b->x + b->w &&
                by + BALL_SIZE >= b->y && by <= b->y + b->h) {

                /* Clear hit brick */
                b->alive = false;
                s_bricks_remaining--;
                s_score += b->points;
                if (s_score > s_high_score) {
                    s_high_score = s_score;
                }

                /* Determine rebound normal from previous tick position */
                int prev_by = FP_TO_INT(s_ball_y_fp - s_ball_vy_fp);
                int prev_bx = FP_TO_INT(s_ball_x_fp - s_ball_vx_fp);

                if (prev_by + BALL_SIZE <= b->y) {
                    /* Collided from top */
                    by = b->y - BALL_SIZE;
                    s_ball_y_fp = INT_TO_FP(by);
                    if (s_ball_vy_fp > 0) s_ball_vy_fp = -s_ball_vy_fp;
                } else if (prev_by >= b->y + b->h) {
                    /* Collided from bottom */
                    by = b->y + b->h;
                    s_ball_y_fp = INT_TO_FP(by);
                    if (s_ball_vy_fp < 0) s_ball_vy_fp = -s_ball_vy_fp;
                } else if (prev_bx + BALL_SIZE <= b->x) {
                    /* Collided from left edge */
                    bx = b->x - BALL_SIZE;
                    s_ball_x_fp = INT_TO_FP(bx);
                    if (s_ball_vx_fp > 0) s_ball_vx_fp = -s_ball_vx_fp;
                } else if (prev_bx >= b->x + b->w) {
                    /* Collided from right edge */
                    bx = b->x + b->w;
                    s_ball_x_fp = INT_TO_FP(bx);
                    if (s_ball_vx_fp < 0) s_ball_vx_fp = -s_ball_vx_fp;
                } else {
                    /* Default reverse vertical direction */
                    s_ball_vy_fp = -s_ball_vy_fp;
                }

                /* Check for complete level clearance */
                if (s_bricks_remaining == 0) {
                    s_state = STATE_VICTORY;
                }

                /* One brick impact per physics step */
                goto collision_resolved;
            }
        }
    }
collision_resolved:

    /* Redraw arena with updated ball and brick states */
    breakout_redraw_all();
}

/*
 * breakout_app_create:
 * Creates the Breakout window, initializes game state, and registers with window manager.
 */
window_t* breakout_app_create(int32_t x, int32_t y) {
    if (s_breakout_win) {
        wm_bring_to_front(s_breakout_win);
        breakout_redraw_all();
        return s_breakout_win;
    }

    s_breakout_win = window_create((int)x, (int)y, BREAKOUT_WIN_W, BREAKOUT_WIN_H,
                                   "Breakout - 8-Bit Brick Breaker",
                                   WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_breakout_win) {
        serial_writestring("[BREAKOUT] Error: Failed to create window\n");
        return NULL;
    }

    s_breakout_win->on_close = on_breakout_close;
    s_breakout_win->on_key   = on_breakout_key;
    s_breakout_win->on_click = on_breakout_click;

    breakout_reset_game();
    wm_add_window(s_breakout_win);
    breakout_redraw_all();

    serial_writestring("[BREAKOUT] Breakout - 8-Bit Brick Breaker opened\n");
    return s_breakout_win;
}

/*
 * app_breakout_open:
 * Desktop launcher convenience wrapper.
 */
void app_breakout_open(void) {
    if (s_breakout_win) {
        wm_bring_to_front(s_breakout_win);
        breakout_redraw_all();
        return;
    }
    breakout_app_create(220, 90);
}
