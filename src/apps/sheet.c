#include "apps/sheet.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/serial.h"
#include "lib/string.h"
#include "lib/kprintf.h"

#define SHEET_COLS           6
#define SHEET_ROWS           10
#define SHEET_CELL_TEXT_MAX  32

#define SHEET_GRID_X         10
#define SHEET_GRID_Y         40
#define SHEET_HEADER_COL_W   32
#define SHEET_CELL_W         88
#define SHEET_CELL_H         22

/* Cell Structure */
typedef struct {
    char    raw_text[SHEET_CELL_TEXT_MAX];
    int32_t computed_val;
    bool    is_numeric;
    bool    is_error;
} sheet_cell_t;

/* Spreadsheet State */
static window_t*    s_sheet_win      = NULL;
static widget_t*    s_lbl_cell_addr  = NULL;
static widget_t*    s_txt_formula    = NULL;

static sheet_cell_t s_cells[SHEET_ROWS][SHEET_COLS];
static int          s_active_col     = 0;
static int          s_active_row     = 0;

/* Helper: case-insensitive character comparison */
static inline char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

/* Helper: converts integer to string */
static void int_to_str(int32_t val, char* buf) {
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

/* Helper: simple string to integer parser */
static bool parse_int(const char* s, int32_t* out_val) {
    if (!s || !*s) return false;

    int32_t sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    if (!*s) return false;

    int32_t val = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return false;
        val = val * 10 + (*s - '0');
        s++;
    }
    *out_val = val * sign;
    return true;
}

/* Parses a cell coordinate string like "A1", "C4" */
static bool parse_cell_ref(const char* s, int* out_col, int* out_row) {
    if (!s || !s[0] || !s[1]) return false;

    char c = to_upper(s[0]);
    if (c < 'A' || c > 'F') return false;
    *out_col = c - 'A';

    int row = 0;
    int idx = 1;
    while (s[idx] >= '0' && s[idx] <= '9') {
        row = row * 10 + (s[idx] - '0');
        idx++;
    }

    if (row < 1 || row > SHEET_ROWS) return false;
    *out_row = row - 1;
    return true;
}

/* Forward declaration for grid rendering */
static void redraw_sheet_grid(void);

/*
 * evaluate_cell_formula:
 * Evaluates formula string starting with '='.
 * Supports:
 *   =SUM(A1:A5)
 *   =AVG(A1:A5)
 *   =MIN(A1:A5)
 *   =MAX(A1:A5)
 *   =A1+B1, =A1*2, =C3-10, =D2/D1
 */
static void evaluate_formula(sheet_cell_t* cell, const char* formula, int cur_r, int cur_c) {
    cell->is_error = false;
    cell->is_numeric = true;

    /* Skip leading '=' and whitespace */
    const char* p = formula + 1;
    while (*p == ' ') p++;

    /* 1. Range functions: SUM, AVG, MIN, MAX */
    char func[4];
    for (int i = 0; i < 3; i++) {
        func[i] = to_upper(p[i]);
    }
    func[3] = '\0';

    if (p[3] == '(') {
        const char* range_str = p + 4;
        /* Find colon ':' */
        const char* colon = range_str;
        while (*colon && *colon != ':' && *colon != ')') colon++;

        if (*colon == ':') {
            char start_ref[8];
            int s_len = colon - range_str;
            if (s_len >= 7) { cell->is_error = true; return; }
            strncpy(start_ref, range_str, s_len);
            start_ref[s_len] = '\0';

            const char* end_ptr = colon + 1;
            const char* close_paren = end_ptr;
            while (*close_paren && *close_paren != ')') close_paren++;

            char end_ref[8];
            int e_len = close_paren - end_ptr;
            if (e_len >= 7) { cell->is_error = true; return; }
            strncpy(end_ref, end_ptr, e_len);
            end_ref[e_len] = '\0';

            int c1 = 0, r1 = 0, c2 = 0, r2 = 0;
            if (parse_cell_ref(start_ref, &c1, &r1) && parse_cell_ref(end_ref, &c2, &r2)) {
                int min_c = (c1 < c2) ? c1 : c2;
                int max_c = (c1 > c2) ? c1 : c2;
                int min_r = (r1 < r2) ? r1 : r2;
                int max_r = (r1 > r2) ? r1 : r2;

                int32_t sum = 0;
                int32_t count = 0;
                int32_t min_v = 2147483647;
                int32_t max_v = -2147483647;

                for (int r = min_r; r <= max_r; r++) {
                    for (int c = min_c; c <= max_c; c++) {
                        if (r == cur_r && c == cur_c) continue; /* Avoid self */
                        if (s_cells[r][c].is_numeric && !s_cells[r][c].is_error) {
                            int32_t v = s_cells[r][c].computed_val;
                            sum += v;
                            count++;
                            if (v < min_v) min_v = v;
                            if (v > max_v) max_v = v;
                        }
                    }
                }

                if (strcmp(func, "SUM") == 0) {
                    cell->computed_val = sum;
                } else if (strcmp(func, "AVG") == 0) {
                    cell->computed_val = (count > 0) ? (sum / count) : 0;
                } else if (strcmp(func, "MIN") == 0) {
                    cell->computed_val = (count > 0) ? min_v : 0;
                } else if (strcmp(func, "MAX") == 0) {
                    cell->computed_val = (count > 0) ? max_v : 0;
                } else {
                    cell->is_error = true;
                }
                return;
            }
        }
    }

    /* 2. Binary arithmetic expression: [Operand 1] [op] [Operand 2] */
    const char* op_ptr = p;
    char op = 0;
    while (*op_ptr) {
        if (*op_ptr == '+' || *op_ptr == '-' || *op_ptr == '*' || *op_ptr == '/') {
            if (op_ptr > p) { /* Not a leading sign */
                op = *op_ptr;
                break;
            }
        }
        op_ptr++;
    }

    if (op != 0) {
        char op1_str[16];
        int l1 = op_ptr - p;
        if (l1 >= 15) { cell->is_error = true; return; }
        strncpy(op1_str, p, l1);
        op1_str[l1] = '\0';

        const char* op2_str = op_ptr + 1;

        int32_t val1 = 0, val2 = 0;
        int c = 0, r = 0;

        /* Resolve Operand 1 */
        if (parse_cell_ref(op1_str, &c, &r)) {
            val1 = s_cells[r][c].computed_val;
        } else if (!parse_int(op1_str, &val1)) {
            cell->is_error = true;
            return;
        }

        /* Resolve Operand 2 */
        if (parse_cell_ref(op2_str, &c, &r)) {
            val2 = s_cells[r][c].computed_val;
        } else if (!parse_int(op2_str, &val2)) {
            cell->is_error = true;
            return;
        }

        /* Compute binary operation */
        switch (op) {
            case '+': cell->computed_val = val1 + val2; break;
            case '-': cell->computed_val = val1 - val2; break;
            case '*': cell->computed_val = val1 * val2; break;
            case '/':
                if (val2 == 0) {
                    cell->is_error = true;
                    return;
                }
                cell->computed_val = val1 / val2;
                break;
            default: cell->is_error = true; return;
        }
        return;
    }

    /* 3. Single Cell Reference or Literal Number */
    int ref_c = 0, ref_r = 0;
    if (parse_cell_ref(p, &ref_c, &ref_r)) {
        cell->computed_val = s_cells[ref_r][ref_c].computed_val;
        cell->is_numeric = s_cells[ref_r][ref_c].is_numeric;
        cell->is_error = s_cells[ref_r][ref_c].is_error;
        return;
    }

    int32_t literal = 0;
    if (parse_int(p, &literal)) {
        cell->computed_val = literal;
        cell->is_numeric = true;
        return;
    }

    cell->is_error = true;
}

/*
 * sheet_recalculate_all:
 * Evaluates all cell formulas and updates values across the matrix.
 */
static void sheet_recalculate_all(void) {
    /* Pass 1: Parse constant values and raw text */
    for (int r = 0; r < SHEET_ROWS; r++) {
        for (int c = 0; c < SHEET_COLS; c++) {
            sheet_cell_t* cell = &s_cells[r][c];
            if (cell->raw_text[0] != '=') {
                cell->is_error = false;
                int32_t val = 0;
                if (parse_int(cell->raw_text, &val)) {
                    cell->computed_val = val;
                    cell->is_numeric = true;
                } else {
                    cell->computed_val = 0;
                    cell->is_numeric = false;
                }
            }
        }
    }

    /* Pass 2: Evaluate formulas (two passes for simple dependency resolution) */
    for (int pass = 0; pass < 2; pass++) {
        for (int r = 0; r < SHEET_ROWS; r++) {
            for (int c = 0; c < SHEET_COLS; c++) {
                sheet_cell_t* cell = &s_cells[r][c];
                if (cell->raw_text[0] == '=') {
                    evaluate_formula(cell, cell->raw_text, r, c);
                }
            }
        }
    }
}

/* Updates the active cell indicator and formula bar */
static void update_active_cell_ui(void) {
    if (!s_lbl_cell_addr || !s_txt_formula) return;

    /* Update address label: e.g. "B3" */
    char addr[4];
    addr[0] = (char)('A' + s_active_col);
    addr[1] = (char)('1' + s_active_row);
    if (s_active_row == 9) {
        addr[1] = '1';
        addr[2] = '0';
        addr[3] = '\0';
    } else {
        addr[2] = '\0';
    }
    widget_set_text(s_lbl_cell_addr, addr);

    /* Update formula bar text */
    widget_set_text(s_txt_formula, s_cells[s_active_row][s_active_col].raw_text);
}

/*
 * redraw_sheet_grid:
 * Renders spreadsheet headers, grid lines, cells, and active selection outline.
 */
static void redraw_sheet_grid(void) {
    if (!s_sheet_win) return;

    int gx = SHEET_GRID_X;
    int gy = SHEET_GRID_Y;

    /* 1. Header Corner Box */
    window_fill_rect(s_sheet_win, gx, gy, SHEET_HEADER_COL_W, SHEET_CELL_H, 0x00E0E0E0);
    window_draw_rect(s_sheet_win, gx, gy, SHEET_HEADER_COL_W, SHEET_CELL_H, 0x00B0B0B0);

    /* 2. Column Headers (A, B, C, D, E, F) */
    for (int c = 0; c < SHEET_COLS; c++) {
        int cx = gx + SHEET_HEADER_COL_W + c * SHEET_CELL_W;
        window_fill_rect(s_sheet_win, cx, gy, SHEET_CELL_W, SHEET_CELL_H, 0x00E0E0E0);
        window_draw_rect(s_sheet_win, cx, gy, SHEET_CELL_W, SHEET_CELL_H, 0x00B0B0B0);

        char col_str[2] = { (char)('A' + c), '\0' };
        window_draw_string(s_sheet_win, cx + (SHEET_CELL_W - 8) / 2, gy + 4,
                           col_str, 0x00202020, 0x00E0E0E0, true);
    }

    /* 3. Row Headers (1 - 10) & Cell Matrix */
    for (int r = 0; r < SHEET_ROWS; r++) {
        int ry = gy + SHEET_CELL_H + r * SHEET_CELL_H;

        /* Row Number Header */
        window_fill_rect(s_sheet_win, gx, ry, SHEET_HEADER_COL_W, SHEET_CELL_H, 0x00E0E0E0);
        window_draw_rect(s_sheet_win, gx, ry, SHEET_HEADER_COL_W, SHEET_CELL_H, 0x00B0B0B0);

        char row_str[4];
        int_to_str(r + 1, row_str);
        int r_len = (int)strlen(row_str);
        window_draw_string(s_sheet_win, gx + (SHEET_HEADER_COL_W - r_len * 8) / 2, ry + 4,
                           row_str, 0x00202020, 0x00E0E0E0, true);

        /* Cells */
        for (int c = 0; c < SHEET_COLS; c++) {
            int cx = gx + SHEET_HEADER_COL_W + c * SHEET_CELL_W;
            sheet_cell_t* cell = &s_cells[r][c];

            /* Cell background */
            uint32_t bg = (r % 2 == 0) ? 0x00FFFFFF : 0x00F8F8F8;
            window_fill_rect(s_sheet_win, cx, ry, SHEET_CELL_W, SHEET_CELL_H, bg);
            window_draw_rect(s_sheet_win, cx, ry, SHEET_CELL_W, SHEET_CELL_H, 0x00D0D0D0);

            /* Cell Text Display */
            if (cell->is_error) {
                window_draw_string(s_sheet_win, cx + 4, ry + 4, "#ERR", 0x00C02020, bg, true);
            } else if (cell->is_numeric) {
                char val_str[16];
                int_to_str(cell->computed_val, val_str);
                /* Right-align numbers */
                int text_w = (int)strlen(val_str) * FONT_GLYPH_WIDTH;
                int tx = cx + SHEET_CELL_W - text_w - 6;
                if (tx < cx + 4) tx = cx + 4;
                window_draw_string(s_sheet_win, tx, ry + 4, val_str, 0x00103060, bg, true);
            } else if (cell->raw_text[0]) {
                /* Left-align text labels */
                window_draw_string(s_sheet_win, cx + 4, ry + 4, cell->raw_text, 0x00101010, bg, true);
            }

            /* Active Cell Selection Highlight */
            if (r == s_active_row && c == s_active_col) {
                window_draw_rect(s_sheet_win, cx, ry, SHEET_CELL_W, SHEET_CELL_H, 0x001F487E);
                window_draw_rect(s_sheet_win, cx + 1, ry + 1, SHEET_CELL_W - 2, SHEET_CELL_H - 2, 0x001F487E);
            }
        }
    }

    /* Invalidate grid area */
    int total_w = SHEET_HEADER_COL_W + SHEET_COLS * SHEET_CELL_W;
    int total_h = SHEET_CELL_H + SHEET_ROWS * SHEET_CELL_H;
    wm_invalidate_rect(s_sheet_win->client_x + gx, s_sheet_win->client_y + gy, total_w, total_h);

    update_active_cell_ui();
}

/* Callback: Formula text box changed */
static void on_formula_changed(widget_t* w, const char* text) {
    (void)w;
    strncpy(s_cells[s_active_row][s_active_col].raw_text, text, SHEET_CELL_TEXT_MAX - 1);
    s_cells[s_active_row][s_active_col].raw_text[SHEET_CELL_TEXT_MAX - 1] = '\0';
    sheet_recalculate_all();
    redraw_sheet_grid();
}

/* Window Key Event Dispatcher */
static void on_sheet_key(window_t* win, char c) {
    (void)win;

    if (c == '\n' || c == '\r') {
        /* Move to row below */
        if (s_active_row < SHEET_ROWS - 1) {
            s_active_row++;
            redraw_sheet_grid();
        }
    } else {
        /* Route typing to formula textbox */
        widget_dispatch_key(win, c);
    }
}

/* Window Mouse Event Hook */
static void on_sheet_mouse_click(int rx, int ry) {
    int gx = SHEET_GRID_X + SHEET_HEADER_COL_W;
    int gy = SHEET_GRID_Y + SHEET_CELL_H;

    if (rx >= gx && rx < gx + SHEET_COLS * SHEET_CELL_W &&
        ry >= gy && ry < gy + SHEET_ROWS * SHEET_CELL_H) {
        
        int col = (rx - gx) / SHEET_CELL_W;
        int row = (ry - gy) / SHEET_CELL_H;

        if (col >= 0 && col < SHEET_COLS && row >= 0 && row < SHEET_ROWS) {
            s_active_col = col;
            s_active_row = row;
            redraw_sheet_grid();
        }
    }
}

/* Callback: Load Sample Budget Data */
static void on_btn_sample_sheet(widget_t* w, void* data) {
    (void)w; (void)data;

    /* Clear all cells */
    for (int r = 0; r < SHEET_ROWS; r++) {
        for (int c = 0; c < SHEET_COLS; c++) {
            s_cells[r][c].raw_text[0] = '\0';
        }
    }

    /* Row 1 (Headers) */
    strcpy(s_cells[0][0].raw_text, "Item");
    strcpy(s_cells[0][1].raw_text, "Qty");
    strcpy(s_cells[0][2].raw_text, "Rate");
    strcpy(s_cells[0][3].raw_text, "Cost");
    strcpy(s_cells[0][4].raw_text, "Spec");

    /* Row 2 (RAM) */
    strcpy(s_cells[1][0].raw_text, "RAM");
    strcpy(s_cells[1][1].raw_text, "4");
    strcpy(s_cells[1][2].raw_text, "25");
    strcpy(s_cells[1][3].raw_text, "=B2*C2");
    strcpy(s_cells[1][4].raw_text, "128MB");

    /* Row 3 (CPU) */
    strcpy(s_cells[2][0].raw_text, "CPU");
    strcpy(s_cells[2][1].raw_text, "1");
    strcpy(s_cells[2][2].raw_text, "120");
    strcpy(s_cells[2][3].raw_text, "=B3*C3");
    strcpy(s_cells[2][4].raw_text, "i686");

    /* Row 4 (VRAM) */
    strcpy(s_cells[3][0].raw_text, "VRAM");
    strcpy(s_cells[3][1].raw_text, "3");
    strcpy(s_cells[3][2].raw_text, "10");
    strcpy(s_cells[3][3].raw_text, "=B4*C4");
    strcpy(s_cells[3][4].raw_text, "1024x768");

    /* Row 5 (UART) */
    strcpy(s_cells[4][0].raw_text, "UART");
    strcpy(s_cells[4][1].raw_text, "2");
    strcpy(s_cells[4][2].raw_text, "15");
    strcpy(s_cells[4][3].raw_text, "=B5*C5");
    strcpy(s_cells[4][4].raw_text, "COM1");

    /* Row 6 (Totals) */
    strcpy(s_cells[5][0].raw_text, "Total");
    strcpy(s_cells[5][3].raw_text, "=SUM(D2:D5)");

    /* Row 7 (Average) */
    strcpy(s_cells[6][0].raw_text, "Avg");
    strcpy(s_cells[6][3].raw_text, "=AVG(D2:D5)");

    /* Row 8 (Min) */
    strcpy(s_cells[7][0].raw_text, "Min");
    strcpy(s_cells[7][3].raw_text, "=MIN(D2:D5)");

    /* Row 9 (Max) */
    strcpy(s_cells[8][0].raw_text, "Max");
    strcpy(s_cells[8][3].raw_text, "=MAX(D2:D5)");

    sheet_recalculate_all();
    s_active_col = 3;
    s_active_row = 5;
    redraw_sheet_grid();
}

/* Window Close Handler */
static void on_sheet_close(window_t* win) {
    wm_remove_window(win);
    s_sheet_win = NULL;
    s_lbl_cell_addr = NULL;
    s_txt_formula = NULL;
}

/* Window Mouse Click Handler */
static void on_sheet_click_wrapper(window_t* win, int rx, int ry) {
    (void)win;
    on_sheet_mouse_click(rx, ry);
}

/*
 * app_sheet_open:
 * Creates and displays the Spreadsheet application window.
 */
void app_sheet_open(void) {
    if (s_sheet_win) {
        wm_bring_to_front(s_sheet_win);
        return;
    }

    s_sheet_win = window_create(200, 100, 580, 330, "Spreadsheet - Sheet1",
                                WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_sheet_win) return;

    s_sheet_win->on_close = on_sheet_close;
    s_sheet_win->on_key   = on_sheet_key;
    s_sheet_win->on_click = on_sheet_click_wrapper;

    /* 1. Formula Bar */
    /* Cell Address indicator box */
    window_fill_rect(s_sheet_win, 10, 8, 38, 26, 0x00E4E8F0);
    window_draw_rect(s_sheet_win, 10, 8, 38, 26, 0x0090A0B8);
    s_lbl_cell_addr = widget_create_label(18, 14, 28, 18, "A1", 0x001F487E);
    widget_add_to_window(s_sheet_win, s_lbl_cell_addr);

    /* Formula input textbox */
    s_txt_formula = widget_create_textbox(52, 6, 410, 28, "", SHEET_CELL_TEXT_MAX);
    s_txt_formula->on_change = on_formula_changed;
    widget_add_to_window(s_sheet_win, s_txt_formula);

    /* Sample Data button */
    widget_add_to_window(s_sheet_win, widget_create_button(470, 6, 96, 28, "Sample", on_btn_sample_sheet, NULL));

    /* Reset cells to empty */
    for (int r = 0; r < SHEET_ROWS; r++) {
        for (int c = 0; c < SHEET_COLS; c++) {
            s_cells[r][c].raw_text[0] = '\0';
            s_cells[r][c].computed_val = 0;
            s_cells[r][c].is_numeric = false;
            s_cells[r][c].is_error = false;
        }
    }

    s_active_col = 0;
    s_active_row = 0;

    redraw_sheet_grid();
    wm_add_window(s_sheet_win);

    serial_writestring("[SHEET] Spreadsheet window opened.\n");
}

/* Hook called from wm mouse dispatcher for cell clicks */
void sheet_handle_client_click(window_t* win, int rx, int ry) {
    if (win == s_sheet_win) {
        on_sheet_mouse_click(rx, ry);
    }
}
