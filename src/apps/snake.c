#include "apps/snake.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define SNAKE_GRID_W      20
#define SNAKE_GRID_H      20
#define SNAKE_CELL_SIZE   14
#define SNAKE_BOARD_X     16
#define SNAKE_BOARD_Y     42
#define SNAKE_BOARD_W     (SNAKE_GRID_W * SNAKE_CELL_SIZE) /* 280 */
#define SNAKE_BOARD_H     (SNAKE_GRID_H * SNAKE_CELL_SIZE) /* 280 */

#define SNAKE_WIN_W       316
#define SNAKE_WIN_H       376
#define SNAKE_MAX_LEN     400

typedef enum {
    DIR_UP = 0,
    DIR_RIGHT,
    DIR_DOWN,
    DIR_LEFT
} snake_dir_t;

typedef struct {
    int x;
    int y;
} point_t;

static window_t*   s_snake_win = NULL;
static widget_t*   s_lbl_score = NULL;

static point_t     s_body[SNAKE_MAX_LEN];
static int         s_length = 4;
static snake_dir_t s_dir = DIR_RIGHT;
static snake_dir_t s_next_dir = DIR_RIGHT;

static point_t     s_food = { 10, 10 };
static bool        s_game_over = false;
static bool        s_paused = false;
static int         s_score = 0;
static int         s_high_score = 0;

/* Helper: formats uint32_t into decimal string */
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

/* Pseudo-random coordinate generator using timer ticks */
static void spawn_food(void) {
    uint32_t seed = timer_get_ticks();
    int fx = (int)((seed ^ 0x5A) % SNAKE_GRID_W);
    int fy = (int)(((seed >> 3) ^ 0xA5) % SNAKE_GRID_H);
    if (fx < 0) fx = -fx;
    if (fy < 0) fy = -fy;

    s_food.x = fx;
    s_food.y = fy;
}

/* Resets game state */
static void snake_reset_game(void) {
    s_length = 4;
    s_body[0].x = 8;  s_body[0].y = 10;
    s_body[1].x = 7;  s_body[1].y = 10;
    s_body[2].x = 6;  s_body[2].y = 10;
    s_body[3].x = 5;  s_body[3].y = 10;

    s_dir = DIR_RIGHT;
    s_next_dir = DIR_RIGHT;
    s_game_over = false;
    s_paused = false;
    s_score = 0;
    spawn_food();
}

/* Updates score bar */
static void snake_update_score_label(void) {
    if (!s_lbl_score) return;
    char text[64] = "Score: ";
    char num[16];
    fmt_uint((uint32_t)s_score, num);
    strlcat(text, num, sizeof(text));
    strlcat(text, " | High: ", sizeof(text));
    fmt_uint((uint32_t)s_high_score, num);
    strlcat(text, num, sizeof(text));
    widget_set_text(s_lbl_score, text);
}

/* Redraws game board */
static void snake_draw_board(void) {
    if (!s_snake_win) return;

    /* 1. Board background */
    window_fill_rect(s_snake_win, SNAKE_BOARD_X, SNAKE_BOARD_Y, SNAKE_BOARD_W, SNAKE_BOARD_H, 0x000F2615);
    window_draw_rect(s_snake_win, SNAKE_BOARD_X - 1, SNAKE_BOARD_Y - 1, SNAKE_BOARD_W + 2, SNAKE_BOARD_H + 2, 0x00336633);

    /* Subtle grid lines */
    for (int i = 1; i < SNAKE_GRID_W; i++) {
        int x = SNAKE_BOARD_X + i * SNAKE_CELL_SIZE;
        for (int j = 0; j < SNAKE_BOARD_H; j += 4) {
            window_putpixel(s_snake_win, x, SNAKE_BOARD_Y + j, 0x0014381C);
        }
    }
    for (int j = 1; j < SNAKE_GRID_H; j++) {
        int y = SNAKE_BOARD_Y + j * SNAKE_CELL_SIZE;
        for (int i = 0; i < SNAKE_BOARD_W; i += 4) {
            window_putpixel(s_snake_win, SNAKE_BOARD_X + i, y, 0x0014381C);
        }
    }

    /* 2. Draw Food (Red Apple) */
    int food_px = SNAKE_BOARD_X + s_food.x * SNAKE_CELL_SIZE;
    int food_py = SNAKE_BOARD_Y + s_food.y * SNAKE_CELL_SIZE;
    window_fill_rect(s_snake_win, food_px + 2, food_py + 3, SNAKE_CELL_SIZE - 4, SNAKE_CELL_SIZE - 5, 0x00FF2020);
    window_fill_rect(s_snake_win, food_px + 5, food_py + 1, 4, 3, 0x0020B020); /* leaf */

    /* 3. Draw Snake Body */
    for (int i = 0; i < s_length; i++) {
        int cx = SNAKE_BOARD_X + s_body[i].x * SNAKE_CELL_SIZE;
        int cy = SNAKE_BOARD_Y + s_body[i].y * SNAKE_CELL_SIZE;

        uint32_t color = (i == 0) ? 0x0000FF99 : 0x0000CC66;
        window_fill_rect(s_snake_win, cx + 1, cy + 1, SNAKE_CELL_SIZE - 2, SNAKE_CELL_SIZE - 2, color);
        window_draw_rect(s_snake_win, cx + 1, cy + 1, SNAKE_CELL_SIZE - 2, SNAKE_CELL_SIZE - 2, 0x00008844);

        /* Head eyes */
        if (i == 0) {
            window_fill_rect(s_snake_win, cx + 4, cy + 4, 2, 2, 0x00000000);
            window_fill_rect(s_snake_win, cx + 8, cy + 4, 2, 2, 0x00000000);
        }
    }

    /* 4. Game Over Overlay */
    if (s_game_over) {
        window_fill_rect(s_snake_win, SNAKE_BOARD_X + 20, SNAKE_BOARD_Y + 110, SNAKE_BOARD_W - 40, 60, 0x00000000);
        window_draw_rect(s_snake_win, SNAKE_BOARD_X + 20, SNAKE_BOARD_Y + 110, SNAKE_BOARD_W - 40, 60, 0x00FF3333);
        window_draw_string(s_snake_win, SNAKE_BOARD_X + 80, SNAKE_BOARD_Y + 120, "GAME OVER!", 0x00FF3333, 0, true);
        window_draw_string(s_snake_win, SNAKE_BOARD_X + 40, SNAKE_BOARD_Y + 144, "Press Space to Restart", 0x00FFFFFF, 0, true);
    }

    /* Invalidate window region to composite immediately */
    wm_invalidate_rect(s_snake_win->x, s_snake_win->y, s_snake_win->width, s_snake_win->height);
}

/* Advances snake one step */
static void snake_step(void) {
    if (s_game_over || s_paused) return;

    s_dir = s_next_dir;
    point_t new_head = s_body[0];

    switch (s_dir) {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    /* Wall collision check */
    if (new_head.x < 0 || new_head.x >= SNAKE_GRID_W ||
        new_head.y < 0 || new_head.y >= SNAKE_GRID_H) {
        s_game_over = true;
        if (s_score > s_high_score) s_high_score = s_score;
        snake_draw_board();
        return;
    }

    /* Self collision check */
    for (int i = 0; i < s_length; i++) {
        if (s_body[i].x == new_head.x && s_body[i].y == new_head.y) {
            s_game_over = true;
            if (s_score > s_high_score) s_high_score = s_score;
            snake_draw_board();
            return;
        }
    }

    /* Move body */
    bool ate_food = (new_head.x == s_food.x && new_head.y == s_food.y);

    if (ate_food) {
        if (s_length < SNAKE_MAX_LEN - 1) {
            s_length++;
        }
        s_score += 10;
        if (s_score > s_high_score) s_high_score = s_score;
        spawn_food();
        snake_update_score_label();
    }

    for (int i = s_length - 1; i > 0; i--) {
        s_body[i] = s_body[i - 1];
    }
    s_body[0] = new_head;

    snake_draw_board();
}

/* Keyboard handler */
static void on_snake_key(window_t* win, char c) {
    (void)win;

    if (s_game_over) {
        if (c == ' ' || c == '\n') {
            snake_reset_game();
            snake_update_score_label();
            snake_draw_board();
        }
        return;
    }

    /* Controls: WASD or arrows */
    if ((c == 'w' || c == 'W') && s_dir != DIR_DOWN) {
        s_next_dir = DIR_UP;
    } else if ((c == 's' || c == 'S') && s_dir != DIR_UP) {
        s_next_dir = DIR_DOWN;
    } else if ((c == 'a' || c == 'A') && s_dir != DIR_RIGHT) {
        s_next_dir = DIR_LEFT;
    } else if ((c == 'd' || c == 'D') && s_dir != DIR_LEFT) {
        s_next_dir = DIR_RIGHT;
    } else if (c == 'p' || c == 'P') {
        s_paused = !s_paused;
        if (s_lbl_score) {
            widget_set_text(s_lbl_score, s_paused ? "[ PAUSED ] - Press P" : "Resumed");
        }
    }

    /* Immediately advance tick on turn key */
    snake_step();
}

/* Button: New Game */
static void on_btn_new_game(widget_t* w, void* data) {
    (void)w; (void)data;
    snake_reset_game();
    snake_update_score_label();
    snake_draw_board();
}

/* Window Close */
static void on_snake_close(window_t* win) {
    wm_remove_window(win);
    s_snake_win = NULL;
    s_lbl_score = NULL;
}

/*
 * app_snake_open:
 * Opens or restores Snake Arcade window.
 */
void app_snake_open(void) {
    if (s_snake_win) {
        wm_bring_to_front(s_snake_win);
        snake_draw_board();
        return;
    }

    s_snake_win = window_create(220, 80, SNAKE_WIN_W, SNAKE_WIN_H,
                                "Snake Arcade",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_snake_win) return;

    s_snake_win->on_close = on_snake_close;
    s_snake_win->on_key   = on_snake_key;

    /* Score label */
    s_lbl_score = widget_create_label(16, 12, 180, 20, "Score: 0 | High: 0", 0x000F3018);
    widget_add_to_window(s_snake_win, s_lbl_score);

    /* Restart button */
    widget_add_to_window(s_snake_win,
        widget_create_button(206, 8, 94, 26, "New Game", on_btn_new_game, NULL));

    snake_reset_game();
    wm_add_window(s_snake_win);
    snake_draw_board();

    serial_writestring("[SNAKE] Snake Arcade window opened\n");
}
