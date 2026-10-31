#include "apps/minesweeper.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "drivers/timer.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/*
 * minesweeper.c:
 * Classic Windows Minesweeper desktop puzzle application for MyOS.
 *
 * Why this is needed:
 * Implements the beloved 9x9, 10-mine strategic deduction puzzle game.
 * Features an authentic Windows 95 aesthetic with 3D beveled surfaces,
 * a 7-segment digital mine counter and timer, an interactive yellow smiley face
 * button that reflects game status (normal, cool sunglasses on win, dead X-eyes on loss),
 * recursive zero-mine flood fill, and a flag toggle mechanism.
 */

#define MINE_WIN_WIDTH         280
#define MINE_WIN_HEIGHT        360

#define MINE_GRID_SIZE         9
#define MINE_TOTAL_MINES       10
#define MINE_TOTAL_SAFE_CELLS  (MINE_GRID_SIZE * MINE_GRID_SIZE - MINE_TOTAL_MINES) /* 71 */
#define MINE_CELL_SIZE         26

/* Coordinate layout inside the 276 x 334 window client area */
#define MINE_HEADER_X          12
#define MINE_HEADER_Y          8
#define MINE_HEADER_W          252
#define MINE_HEADER_H          34

#define MINE_COUNTER_X         16
#define MINE_COUNTER_Y         12
#define MINE_COUNTER_W         48
#define MINE_COUNTER_H         26

#define MINE_SMILEY_X          122
#define MINE_SMILEY_Y          10
#define MINE_SMILEY_W          32
#define MINE_SMILEY_H          30

#define MINE_TIMER_X           212
#define MINE_TIMER_Y           12
#define MINE_TIMER_W           48
#define MINE_TIMER_H           26

#define MINE_GRID_X            21
#define MINE_GRID_Y            76
#define MINE_GRID_W            (MINE_GRID_SIZE * MINE_CELL_SIZE) /* 234 */
#define MINE_GRID_H            (MINE_GRID_SIZE * MINE_CELL_SIZE) /* 234 */

/* Color Palette Constants (Classic 32-bit RGB) */
#define MINE_COLOR_BG          0x00C0C0C0  /* Windows 95 gray */
#define MINE_COLOR_WHITE       0x00FFFFFF  /* 3D highlight */
#define MINE_COLOR_DARKGRAY    0x00808080  /* 3D shadow */
#define MINE_COLOR_SHADOW      0x00707070  /* Deeper shadow */
#define MINE_COLOR_BLACK       0x00000000  /* Text and borders */
#define MINE_COLOR_LED_RED     0x00FF0000  /* Digital readout digits */
#define MINE_COLOR_REVEALED_BG 0x00BDBDBD  /* Sunken revealed cell fill */
#define MINE_COLOR_EXPLODED_BG 0x00E02020  /* Red background for detonated mine */
#define MINE_COLOR_FLAG_RED    0x00E02020  /* Red triangular flag banner */
#define MINE_COLOR_SMILEY_YEL  0x00FFD700  /* Golden yellow smiley face */

/* Standard Minesweeper number glyph colors (1 through 8) */
static const uint32_t s_number_colors[9] = {
    0x00000000, /* 0: Unused */
    0x000000FF, /* 1: Blue */
    0x00008000, /* 2: Green */
    0x00E00000, /* 3: Red */
    0x00000080, /* 4: Dark Blue */
    0x00800000, /* 5: Maroon */
    0x00008080, /* 6: Teal */
    0x00000000, /* 7: Black */
    0x00808080  /* 8: Gray */
};

/* Cell reveal and flag states */
typedef enum {
    CELL_COVERED = 0,
    CELL_REVEALED,
    CELL_FLAGGED
} cell_state_t;

/* Overall game progression states */
typedef enum {
    GAME_NOT_STARTED = 0,
    GAME_PLAYING,
    GAME_WON,
    GAME_LOST
} game_state_t;

/* Single board cell definition */
typedef struct {
    bool         is_mine;
    bool         is_exploded;
    cell_state_t state;
    uint8_t      adj_mines;
} cell_t;

/* Position coordinate for BFS flood-fill */
typedef struct {
    int r;
    int c;
} cell_pos_t;

/* Application Static State */
static window_t*    s_mine_win        = NULL;
static widget_t*    s_btn_mode        = NULL;
static cell_t       s_grid[MINE_GRID_SIZE][MINE_GRID_SIZE];

static game_state_t s_game_state      = GAME_NOT_STARTED;
static bool         s_mode_flag       = false; /* false = DIG (reveal), true = FLAG */
static int          s_flags_placed    = 0;
static int          s_revealed_count  = 0;

static uint32_t     s_start_time_sec  = 0;
static uint32_t     s_elapsed_sec     = 0;
static uint32_t     s_rng_state       = 0x4D696E65; /* Seed 'Mine' */

/* Forward declarations */
static void minesweeper_reset_game(void);
static void minesweeper_redraw_all(void);
static void minesweeper_draw_topbar(void);
static void minesweeper_draw_cell(int r, int c);

/*
 * minesweeper_rand:
 * Fast, self-contained 32-bit Xorshift pseudo-random number generator.
 *
 * Why this is needed:
 * The freestanding C kernel has no libc rand(), but mine placement requires
 * unpredictable distribution across the 81 cells based on hardware timer jitter.
 */
static uint32_t minesweeper_rand(void) {
    if (s_rng_state == 0) {
        s_rng_state = timer_get_ticks() ^ 0xA5A55A5A;
    }
    uint32_t x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

/*
 * draw_sunken_border:
 * Draws a 1-pixel 3D sunken border giving recessed depth (dark top-left, white bottom-right).
 */
static void draw_sunken_border(window_t* win, int x, int y, int w, int h) {
    window_fill_rect(win, x, y, w, 1, MINE_COLOR_DARKGRAY);
    window_fill_rect(win, x, y, 1, h, MINE_COLOR_DARKGRAY);
    window_fill_rect(win, x, y + h - 1, w, 1, MINE_COLOR_WHITE);
    window_fill_rect(win, x + w - 1, y, 1, h, MINE_COLOR_WHITE);
}

/*
 * draw_raised_border:
 * Draws a 1-pixel 3D raised border giving push-button height (white top-left, dark bottom-right).
 */
static void draw_raised_border(window_t* win, int x, int y, int w, int h) {
    window_fill_rect(win, x, y, w, 1, MINE_COLOR_WHITE);
    window_fill_rect(win, x, y, 1, h, MINE_COLOR_WHITE);
    window_fill_rect(win, x, y + h - 1, w, 1, MINE_COLOR_SHADOW);
    window_fill_rect(win, x + w - 1, y, 1, h, MINE_COLOR_SHADOW);
}

/*
 * draw_filled_circle:
 * Rasterizes a circle with a smooth 1-pixel border into the window client canvas.
 */
static void draw_filled_circle(window_t* win, int cx, int cy, int r,
                               uint32_t fill_color, uint32_t border_color) {
    int r_sq = r * r;
    int inner_sq = (r - 1) * (r - 1);
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            int d = x * x + y * y;
            if (d <= r_sq) {
                if (d >= inner_sq) {
                    window_putpixel(win, cx + x, cy + y, border_color);
                } else {
                    window_putpixel(win, cx + x, cy + y, fill_color);
                }
            }
        }
    }
}

/*
 * minesweeper_place_mines:
 * Randomly distributes 10 mines across the board, strictly protecting
 * the cell clicked on the first move and its adjacent neighbors.
 *
 * Why this is needed:
 * Classic Windows Minesweeper guarantees that the player's first click is never
 * a mine, ensuring fair strategic play and giving an immediate opening patch.
 */
static void minesweeper_place_mines(int safe_r, int safe_c) {
    int placed = 0;
    while (placed < MINE_TOTAL_MINES) {
        int r = (int)(minesweeper_rand() % MINE_GRID_SIZE);
        int c = (int)(minesweeper_rand() % MINE_GRID_SIZE);

        /* Protect the clicked cell and its immediate 8 neighbors */
        int dr = r - safe_r;
        int dc = c - safe_c;
        if (dr >= -1 && dr <= 1 && dc >= -1 && dc <= 1) {
            continue;
        }

        if (!s_grid[r][c].is_mine) {
            s_grid[r][c].is_mine = true;
            placed++;
        }
    }

    /* Precalculate adjacent mine counts (0 to 8) for every cell */
    for (int r = 0; r < MINE_GRID_SIZE; r++) {
        for (int c = 0; c < MINE_GRID_SIZE; c++) {
            if (s_grid[r][c].is_mine) continue;
            uint8_t count = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr;
                    int nc = c + dc;
                    if (nr >= 0 && nr < MINE_GRID_SIZE && nc >= 0 && nc < MINE_GRID_SIZE) {
                        if (s_grid[nr][nc].is_mine) count++;
                    }
                }
            }
            s_grid[r][c].adj_mines = count;
        }
    }
}

/*
 * minesweeper_reveal_all_mines:
 * Reveals all mines across the board when the game is lost.
 */
static void minesweeper_reveal_all_mines(void) {
    for (int r = 0; r < MINE_GRID_SIZE; r++) {
        for (int c = 0; c < MINE_GRID_SIZE; c++) {
            if (s_grid[r][c].is_mine) {
                if (s_grid[r][c].state != CELL_FLAGGED) {
                    s_grid[r][c].state = CELL_REVEALED;
                }
            }
        }
    }
}

/*
 * minesweeper_reveal_cell:
 * Uncovers a cell, performing an iterative BFS flood fill if the cell has 0 adjacent mines.
 *
 * Why this is needed:
 * If an uncovered cell has no adjacent mines, all 8 neighboring cells are safely revealed.
 * An iterative queue is used instead of recursion to guarantee zero kernel stack exhaustion.
 */
static void minesweeper_reveal_cell(int start_r, int start_c) {
    if (start_r < 0 || start_r >= MINE_GRID_SIZE ||
        start_c < 0 || start_c >= MINE_GRID_SIZE) {
        return;
    }

    cell_t* start_cell = &s_grid[start_r][start_c];
    if (start_cell->state != CELL_COVERED) return;

    /* Hit a mine -> Game Over */
    if (start_cell->is_mine) {
        start_cell->is_exploded = true;
        s_game_state = GAME_LOST;
        minesweeper_reveal_all_mines();
        kprintf("[MINESWEEPER] Detonated mine at (%d,%d)! Game over.\n", start_r, start_c);
        return;
    }

    /* Fixed stack-allocated BFS queue for the 81-cell grid */
    cell_pos_t queue[MINE_GRID_SIZE * MINE_GRID_SIZE];
    int head = 0;
    int tail = 0;

    start_cell->state = CELL_REVEALED;
    s_revealed_count++;

    if (start_cell->adj_mines == 0) {
        queue[tail++] = (cell_pos_t){ start_r, start_c };
    }

    while (head < tail) {
        cell_pos_t cur = queue[head++];
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = cur.r + dr;
                int nc = cur.c + dc;
                if (nr >= 0 && nr < MINE_GRID_SIZE && nc >= 0 && nc < MINE_GRID_SIZE) {
                    cell_t* n = &s_grid[nr][nc];
                    if (n->state == CELL_COVERED && !n->is_mine) {
                        n->state = CELL_REVEALED;
                        s_revealed_count++;
                        if (n->adj_mines == 0) {
                            queue[tail++] = (cell_pos_t){ nr, nc };
                        }
                    }
                }
            }
        }
    }

    /* Check win condition: exactly 71 safe cells revealed */
    if (s_revealed_count >= MINE_TOTAL_SAFE_CELLS) {
        s_game_state = GAME_WON;
        /* Auto-flag any remaining unflagged mines for visual completion */
        for (int r = 0; r < MINE_GRID_SIZE; r++) {
            for (int c = 0; c < MINE_GRID_SIZE; c++) {
                if (s_grid[r][c].is_mine) {
                    s_grid[r][c].state = CELL_FLAGGED;
                }
            }
        }
        s_flags_placed = MINE_TOTAL_MINES;
        kprintf("[MINESWEEPER] Victory! All mines cleared in %u seconds!\n", s_elapsed_sec);
    }
}

/*
 * minesweeper_draw_smiley:
 * Renders the iconic 32x32 yellow smiley button reflecting the current game state.
 */
static void minesweeper_draw_smiley(void) {
    if (!s_mine_win) return;

    /* Raised push-button base */
    window_fill_rect(s_mine_win, MINE_SMILEY_X, MINE_SMILEY_Y, MINE_SMILEY_W, MINE_SMILEY_H, MINE_COLOR_BG);
    draw_raised_border(s_mine_win, MINE_SMILEY_X, MINE_SMILEY_Y, MINE_SMILEY_W, MINE_SMILEY_H);

    int cx = MINE_SMILEY_X + MINE_SMILEY_W / 2;
    int cy = MINE_SMILEY_Y + MINE_SMILEY_H / 2;
    int r = 9;

    /* Yellow face disk */
    draw_filled_circle(s_mine_win, cx, cy, r, MINE_COLOR_SMILEY_YEL, MINE_COLOR_BLACK);

    if (s_game_state == GAME_LOST) {
        /* Dead X-eyes (loss) */
        /* Left Eye 'X' */
        window_putpixel(s_mine_win, cx - 5, cy - 4, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 3, cy - 4, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 4, cy - 3, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 5, cy - 2, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 3, cy - 2, MINE_COLOR_BLACK);

        /* Right Eye 'X' */
        window_putpixel(s_mine_win, cx + 3, cy - 4, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 5, cy - 4, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 4, cy - 3, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 3, cy - 2, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 5, cy - 2, MINE_COLOR_BLACK);

        /* Sad open mouth */
        window_fill_rect(s_mine_win, cx - 3, cy + 2, 7, 4, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx - 2, cy + 3, 5, 2, MINE_COLOR_SMILEY_YEL);
    } else if (s_game_state == GAME_WON) {
        /* Cool sunglasses (victory) */
        window_fill_rect(s_mine_win, cx - 6, cy - 3, 13, 2, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx - 5, cy - 2, 4, 3, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx + 2, cy - 2, 4, 3, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 4, cy - 2, MINE_COLOR_WHITE);
        window_putpixel(s_mine_win, cx + 3, cy - 2, MINE_COLOR_WHITE);

        /* Wide confident smile */
        window_fill_rect(s_mine_win, cx - 4, cy + 3, 9, 2, MINE_COLOR_BLACK);
    } else {
        /* Normal happy eyes and smiling mouth */
        window_fill_rect(s_mine_win, cx - 4, cy - 3, 2, 2, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx + 3, cy - 3, 2, 2, MINE_COLOR_BLACK);

        /* Curved pixel smile */
        window_putpixel(s_mine_win, cx - 4, cy + 2, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx - 3, cy + 3, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx - 2, cy + 4, 5, 1, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 3, cy + 3, MINE_COLOR_BLACK);
        window_putpixel(s_mine_win, cx + 4, cy + 2, MINE_COLOR_BLACK);
    }
}

/*
 * minesweeper_draw_topbar:
 * Renders the top status bar containing the 3-digit mine counter,
 * the interactive smiley button, and the 3-digit elapsed timer.
 */
static void minesweeper_draw_topbar(void) {
    if (!s_mine_win) return;

    /* 1. Recessed header box containing counter, smiley, and timer */
    window_fill_rect(s_mine_win, MINE_HEADER_X, MINE_HEADER_Y, MINE_HEADER_W, MINE_HEADER_H, MINE_COLOR_BG);
    draw_sunken_border(s_mine_win, MINE_HEADER_X, MINE_HEADER_Y, MINE_HEADER_W, MINE_HEADER_H);

    /* 2. Mine Counter Display Box (Sunken black display) */
    window_fill_rect(s_mine_win, MINE_COUNTER_X, MINE_COUNTER_Y, MINE_COUNTER_W, MINE_COUNTER_H, MINE_COLOR_BLACK);
    draw_sunken_border(s_mine_win, MINE_COUNTER_X, MINE_COUNTER_Y, MINE_COUNTER_W, MINE_COUNTER_H);

    int remaining = MINE_TOTAL_MINES - s_flags_placed;
    char count_str[4];
    if (remaining < 0) {
        int abs_r = -remaining;
        if (abs_r > 99) abs_r = 99;
        count_str[0] = '-';
        count_str[1] = (char)('0' + (abs_r / 10));
        count_str[2] = (char)('0' + (abs_r % 10));
    } else {
        if (remaining > 999) remaining = 999;
        count_str[0] = (char)('0' + (remaining / 100));
        count_str[1] = (char)('0' + ((remaining / 10) % 10));
        count_str[2] = (char)('0' + (remaining % 10));
    }
    count_str[3] = '\0';

    int txt_x = MINE_COUNTER_X + (MINE_COUNTER_W - (3 * FONT_GLYPH_WIDTH)) / 2;
    int txt_y = MINE_COUNTER_Y + (MINE_COUNTER_H - FONT_GLYPH_HEIGHT) / 2;
    window_draw_string(s_mine_win, txt_x, txt_y, count_str, MINE_COLOR_LED_RED, MINE_COLOR_BLACK, false);

    /* 3. Central Smiley Face Button */
    minesweeper_draw_smiley();

    /* 4. Timer Display Box (Sunken black display) */
    window_fill_rect(s_mine_win, MINE_TIMER_X, MINE_TIMER_Y, MINE_TIMER_W, MINE_TIMER_H, MINE_COLOR_BLACK);
    draw_sunken_border(s_mine_win, MINE_TIMER_X, MINE_TIMER_Y, MINE_TIMER_W, MINE_TIMER_H);

    char timer_str[4];
    uint32_t t = s_elapsed_sec;
    if (t > 999) t = 999;
    timer_str[0] = (char)('0' + (t / 100));
    timer_str[1] = (char)('0' + ((t / 10) % 10));
    timer_str[2] = (char)('0' + (t % 10));
    timer_str[3] = '\0';

    int t_x = MINE_TIMER_X + (MINE_TIMER_W - (3 * FONT_GLYPH_WIDTH)) / 2;
    int t_y = MINE_TIMER_Y + (MINE_TIMER_H - FONT_GLYPH_HEIGHT) / 2;
    window_draw_string(s_mine_win, t_x, t_y, timer_str, MINE_COLOR_LED_RED, MINE_COLOR_BLACK, false);
}

/*
 * minesweeper_draw_cell:
 * Renders an individual cell (covered, flagged, or revealed number/mine).
 */
static void minesweeper_draw_cell(int r, int c) {
    if (!s_mine_win) return;

    int cx = MINE_GRID_X + c * MINE_CELL_SIZE;
    int cy = MINE_GRID_Y + r * MINE_CELL_SIZE;
    cell_t* cell = &s_grid[r][c];

    if (cell->state == CELL_COVERED) {
        /* Raised 3D push button */
        window_fill_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, MINE_CELL_SIZE, MINE_COLOR_BG);
        /* 2-pixel bevel for authentic chunky Windows 95 feel */
        window_fill_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, 2, MINE_COLOR_WHITE);
        window_fill_rect(s_mine_win, cx, cy, 2, MINE_CELL_SIZE, MINE_COLOR_WHITE);
        window_fill_rect(s_mine_win, cx, cy + MINE_CELL_SIZE - 2, MINE_CELL_SIZE, 2, MINE_COLOR_SHADOW);
        window_fill_rect(s_mine_win, cx + MINE_CELL_SIZE - 2, cy, 2, MINE_CELL_SIZE, MINE_COLOR_SHADOW);
    } else if (cell->state == CELL_FLAGGED) {
        /* Raised 3D button with red flag */
        window_fill_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, MINE_CELL_SIZE, MINE_COLOR_BG);
        window_fill_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, 2, MINE_COLOR_WHITE);
        window_fill_rect(s_mine_win, cx, cy, 2, MINE_CELL_SIZE, MINE_COLOR_WHITE);
        window_fill_rect(s_mine_win, cx, cy + MINE_CELL_SIZE - 2, MINE_CELL_SIZE, 2, MINE_COLOR_SHADOW);
        window_fill_rect(s_mine_win, cx + MINE_CELL_SIZE - 2, cy, 2, MINE_CELL_SIZE, MINE_COLOR_SHADOW);

        /* Flagstaff (mast) and base */
        window_fill_rect(s_mine_win, cx + 9, cy + 6, 2, 14, MINE_COLOR_BLACK);
        window_fill_rect(s_mine_win, cx + 6, cy + 18, 8, 2, MINE_COLOR_BLACK);

        /* Triangular red pennant */
        for (int i = 0; i < 7; i++) {
            window_fill_rect(s_mine_win, cx + 11, cy + 6 + i, 7 - i, 1, MINE_COLOR_FLAG_RED);
        }
    } else {
        /* CELL_REVEALED: Sunken flat surface */
        uint32_t bg = cell->is_exploded ? MINE_COLOR_EXPLODED_BG : MINE_COLOR_REVEALED_BG;
        window_fill_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, MINE_CELL_SIZE, bg);
        window_draw_rect(s_mine_win, cx, cy, MINE_CELL_SIZE, MINE_CELL_SIZE, MINE_COLOR_DARKGRAY);

        if (cell->is_mine) {
            /* Naval Mine graphic: center sphere, white glint, and 8 radial spikes */
            int mx = cx + MINE_CELL_SIZE / 2;
            int my = cy + MINE_CELL_SIZE / 2;

            /* Spikes */
            window_fill_rect(s_mine_win, mx - 1, cy + 5, 2, 16, MINE_COLOR_BLACK);
            window_fill_rect(s_mine_win, cx + 5, my - 1, 16, 2, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx - 5, my - 5, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx - 4, my - 4, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx + 4, my + 4, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx + 5, my + 5, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx + 5, my - 5, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx + 4, my - 4, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx - 4, my + 4, MINE_COLOR_BLACK);
            window_putpixel(s_mine_win, mx - 5, my + 5, MINE_COLOR_BLACK);

            /* Center sphere */
            draw_filled_circle(s_mine_win, mx, my, 5, MINE_COLOR_BLACK, MINE_COLOR_BLACK);
            /* White reflection shine */
            window_putpixel(s_mine_win, mx - 2, my - 2, MINE_COLOR_WHITE);
        } else if (cell->adj_mines > 0) {
            /* Adjacent mine number text */
            char n_str[2];
            n_str[0] = (char)('0' + cell->adj_mines);
            n_str[1] = '\0';
            uint32_t col = s_number_colors[cell->adj_mines];
            int tx = cx + (MINE_CELL_SIZE - FONT_GLYPH_WIDTH) / 2;
            int ty = cy + (MINE_CELL_SIZE - FONT_GLYPH_HEIGHT) / 2;
            window_draw_string(s_mine_win, tx, ty, n_str, col, bg, true);
        }
    }
}

/*
 * minesweeper_redraw_all:
 * Completely redraws the window client canvas and flushes the dirty area to screen.
 */
static void minesweeper_redraw_all(void) {
    if (!s_mine_win) return;

    /* Base background */
    window_clear_client(s_mine_win, MINE_COLOR_BG);

    /* 1. Header controls (mine counter, smiley button, timer) */
    minesweeper_draw_topbar();

    /* 2. Child widgets (Mode toggle button & Reset button) */
    widget_render_all(s_mine_win);

    /* 3. Sunken border frame around the 9x9 board */
    window_fill_rect(s_mine_win, MINE_GRID_X - 2, MINE_GRID_Y - 2, MINE_GRID_W + 4, MINE_GRID_H + 4, MINE_COLOR_BG);
    draw_sunken_border(s_mine_win, MINE_GRID_X - 2, MINE_GRID_Y - 2, MINE_GRID_W + 4, MINE_GRID_H + 4);

    /* 4. Draw all cells */
    for (int r = 0; r < MINE_GRID_SIZE; r++) {
        for (int c = 0; c < MINE_GRID_SIZE; c++) {
            minesweeper_draw_cell(r, c);
        }
    }

    /* 5. Bottom informational status bar */
    const char* status_msg;
    uint32_t status_col;
    if (s_game_state == GAME_WON) {
        status_msg = "** VICTORY! ALL MINES DEFUSED **";
        status_col = 0x00008000;
    } else if (s_game_state == GAME_LOST) {
        status_msg = "** BOOM! GAME OVER - RESET **";
        status_col = 0x00E00000;
    } else if (s_mode_flag) {
        status_msg = "Mode: FLAG | Click cell to mark";
        status_col = 0x00800000;
    } else {
        status_msg = "Mode: DIG | Click cell to reveal";
        status_col = 0x00204060;
    }

    int st_len = (int)strlen(status_msg);
    int st_x = (s_mine_win->client_width - (st_len * FONT_GLYPH_WIDTH)) / 2;
    window_draw_string(s_mine_win, st_x, 314, status_msg, status_col, MINE_COLOR_BG, true);

    /* Invalidate window region to composite to screen */
    wm_invalidate_rect(s_mine_win->x, s_mine_win->y, s_mine_win->width, s_mine_win->height);
}

/*
 * minesweeper_update_timer:
 * Updates the elapsed timer display while a game is active.
 */
static void minesweeper_update_timer(void) {
    if (s_game_state == GAME_PLAYING) {
        uint32_t now_sec = timer_get_uptime_sec();
        uint32_t elapsed = (now_sec >= s_start_time_sec) ? (now_sec - s_start_time_sec) : 0;
        if (elapsed > 999) elapsed = 999;
        if (elapsed != s_elapsed_sec) {
            s_elapsed_sec = elapsed;
            minesweeper_draw_topbar();
            wm_invalidate_rect(s_mine_win->client_x + MINE_TIMER_X,
                               s_mine_win->client_y + MINE_TIMER_Y,
                               MINE_TIMER_W, MINE_TIMER_H);
        }
    }
}

/*
 * minesweeper_reset_game:
 * Clears board cells, resets counters, and returns to initial ready state.
 */
static void minesweeper_reset_game(void) {
    for (int r = 0; r < MINE_GRID_SIZE; r++) {
        for (int c = 0; c < MINE_GRID_SIZE; c++) {
            s_grid[r][c].is_mine     = false;
            s_grid[r][c].is_exploded = false;
            s_grid[r][c].state       = CELL_COVERED;
            s_grid[r][c].adj_mines   = 0;
        }
    }

    s_game_state     = GAME_NOT_STARTED;
    s_flags_placed   = 0;
    s_revealed_count = 0;
    s_elapsed_sec    = 0;

    /* Re-seed RNG with timer entropy */
    s_rng_state ^= timer_get_ticks() ^ (timer_get_uptime_sec() << 16);
    serial_writestring("[MINESWEEPER] Game state reset. Ready for first click.\n");
}

/*
 * on_btn_mode_toggle:
 * Toggles between DIG (reveal) and FLAG (mark) mode.
 */
static void on_btn_mode_toggle(widget_t* w, void* user_data) {
    (void)user_data;
    s_mode_flag = !s_mode_flag;
    if (s_mode_flag) {
        widget_set_text(w, "Mode: FLAG (Mark)");
    } else {
        widget_set_text(w, "Mode: DIG (Reveal)");
    }
    minesweeper_redraw_all();
}

/*
 * on_btn_reset:
 * Reset button click callback.
 */
static void on_btn_reset(widget_t* w, void* user_data) {
    (void)w;
    (void)user_data;
    minesweeper_reset_game();
    minesweeper_redraw_all();
}

/*
 * on_minesweeper_click:
 * Handles mouse clicks inside the window client canvas.
 *
 * Why this is needed:
 * Dispatches clicks to either the reset smiley face button or the 9x9 game grid.
 * Respects flag protection and safely starts the game on the first click.
 */
static void on_minesweeper_click(window_t* win, int rx, int ry) {
    (void)win;

    /* 1. Check Smiley face button hit */
    if (rx >= MINE_SMILEY_X && rx < MINE_SMILEY_X + MINE_SMILEY_W &&
        ry >= MINE_SMILEY_Y && ry < MINE_SMILEY_Y + MINE_SMILEY_H) {
        minesweeper_reset_game();
        minesweeper_redraw_all();
        return;
    }

    /* 2. Check 9x9 Grid hit */
    if (rx >= MINE_GRID_X && rx < MINE_GRID_X + MINE_GRID_W &&
        ry >= MINE_GRID_Y && ry < MINE_GRID_Y + MINE_GRID_H) {

        /* If game is already won or lost, clicks on board are ignored until reset */
        if (s_game_state == GAME_WON || s_game_state == GAME_LOST) {
            return;
        }

        int c = (rx - MINE_GRID_X) / MINE_CELL_SIZE;
        int r = (ry - MINE_GRID_Y) / MINE_CELL_SIZE;
        if (r < 0 || r >= MINE_GRID_SIZE || c < 0 || c >= MINE_GRID_SIZE) return;

        cell_t* cell = &s_grid[r][c];

        if (s_mode_flag) {
            /* FLAG Mode: Place or remove flag on covered cells */
            if (cell->state == CELL_COVERED) {
                cell->state = CELL_FLAGGED;
                s_flags_placed++;
            } else if (cell->state == CELL_FLAGGED) {
                cell->state = CELL_COVERED;
                s_flags_placed--;
            }
        } else {
            /* DIG Mode: Reveal cell */
            /* Flagged cells CANNOT be revealed by left click */
            if (cell->state == CELL_FLAGGED) {
                return;
            }

            if (cell->state == CELL_COVERED) {
                /* First click initialization */
                if (s_game_state == GAME_NOT_STARTED) {
                    minesweeper_place_mines(r, c);
                    s_game_state = GAME_PLAYING;
                    s_start_time_sec = timer_get_uptime_sec();
                    s_elapsed_sec = 0;
                }

                minesweeper_reveal_cell(r, c);
            }
        }

        minesweeper_redraw_all();
    }
}

/*
 * on_minesweeper_key:
 * Handles keyboard hotkeys ('F' = toggle flag mode, 'R' = reset, 'D' = dig mode).
 */
static void on_minesweeper_key(window_t* win, char c) {
    (void)win;
    minesweeper_update_timer();

    if (c == 'f' || c == 'F') {
        s_mode_flag = !s_mode_flag;
        if (s_btn_mode) {
            widget_set_text(s_btn_mode, s_mode_flag ? "Mode: FLAG (Mark)" : "Mode: DIG (Reveal)");
        }
        minesweeper_redraw_all();
    } else if (c == 'd' || c == 'D') {
        s_mode_flag = false;
        if (s_btn_mode) {
            widget_set_text(s_btn_mode, "Mode: DIG (Reveal)");
        }
        minesweeper_redraw_all();
    } else if (c == 'r' || c == 'R' || c == ' ') {
        minesweeper_reset_game();
        minesweeper_redraw_all();
    }
}

/*
 * on_minesweeper_close:
 * Cleans up window and resets state upon window close.
 */
static void on_minesweeper_close(window_t* win) {
    wm_remove_window(win);
    s_mine_win = NULL;
    s_btn_mode = NULL;
    serial_writestring("[MINESWEEPER] Window closed cleanly.\n");
}

/*
 * app_minesweeper_open:
 * Creates and displays the Minesweeper application window.
 */
void app_minesweeper_open(void) {
    if (s_mine_win) {
        wm_bring_to_front(s_mine_win);
        minesweeper_redraw_all();
        return;
    }

    s_mine_win = window_create(220, 90, MINE_WIN_WIDTH, MINE_WIN_HEIGHT,
                               "Minesweeper",
                               WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_mine_win) {
        serial_writestring("[MINESWEEPER] Failed to create window!\n");
        return;
    }

    s_mine_win->bg_color = MINE_COLOR_BG;
    s_mine_win->on_close = on_minesweeper_close;
    s_mine_win->on_key   = on_minesweeper_key;
    s_mine_win->on_click = on_minesweeper_click;

    /* Mode toggle button (172x24) */
    s_btn_mode = widget_create_button(16, 46, 172, 24, "Mode: DIG (Reveal)", on_btn_mode_toggle, NULL);
    widget_add_to_window(s_mine_win, s_btn_mode);

    /* Reset button (70x24) */
    widget_t* btn_reset = widget_create_button(194, 46, 70, 24, "Reset", on_btn_reset, NULL);
    widget_add_to_window(s_mine_win, btn_reset);

    /* Initialize game board */
    minesweeper_reset_game();

    /* Register with window manager */
    wm_add_window(s_mine_win);

    /* Initial render */
    minesweeper_redraw_all();

    serial_writestring("[MINESWEEPER] Minesweeper window initialized and displayed.\n");
}
