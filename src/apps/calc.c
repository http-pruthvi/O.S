#include "apps/calc.h"
#include "gui/window.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "drivers/serial.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* Calculator State */
static window_t* s_calc_win     = NULL;
static widget_t* s_lbl_display  = NULL;

static int32_t   s_accum        = 0;
static int32_t   s_current      = 0;
static char      s_pending_op   = 0;
static bool      s_new_entry    = true;
static bool      s_error        = false;
static char      s_display_str[32] = "0";

/* Helper: formats integer into display string */
static void format_display(int32_t val) {
    if (s_error) return;

    if (val == 0) {
        s_display_str[0] = '0';
        s_display_str[1] = '\0';
        return;
    }

    int idx = 0;
    if (val < 0) {
        s_display_str[idx++] = '-';
        val = -val;
    }

    char temp[32];
    int t_idx = 0;
    while (val > 0) {
        temp[t_idx++] = (char)('0' + (val % 10));
        val /= 10;
    }
    for (int i = t_idx - 1; i >= 0; i--) {
        s_display_str[idx++] = temp[i];
    }
    s_display_str[idx] = '\0';
}

/* Updates the display label widget and triggers window repaint */
static void update_display(void) {
    if (s_lbl_display) {
        widget_set_text(s_lbl_display, s_display_str);
    }
}

/* Performs pending arithmetic calculation */
static void execute_pending(void) {
    if (s_error || s_pending_op == 0) return;

    int32_t result = 0;
    switch (s_pending_op) {
        case '+':
            result = s_accum + s_current;
            break;
        case '-':
            result = s_accum - s_current;
            break;
        case '*':
            result = s_accum * s_current;
            break;
        case '/':
            if (s_current == 0) {
                s_error = true;
                strcpy(s_display_str, "Error: /0");
                update_display();
                return;
            }
            result = s_accum / s_current;
            break;
        default:
            return;
    }

    s_accum = result;
    s_current = result;
    format_display(s_current);
    update_display();
}

/* Handles digit input (0 - 9) */
static void calc_handle_digit(int digit) {
    if (s_error) {
        s_error = false;
        s_accum = 0;
        s_current = 0;
        s_pending_op = 0;
        s_new_entry = true;
    }

    if (s_new_entry) {
        s_current = digit;
        s_new_entry = false;
    } else {
        /* Cap at 9 digits to prevent overflow during entry */
        if (s_current >= 100000000 || s_current <= -100000000) {
            return;
        }
        if (s_current >= 0) {
            s_current = s_current * 10 + digit;
        } else {
            s_current = s_current * 10 - digit;
        }
    }

    format_display(s_current);
    update_display();
}

/* Handles arithmetic operator (+, -, *, /) */
static void calc_handle_op(char op) {
    if (s_error) return;

    if (s_pending_op != 0 && !s_new_entry) {
        execute_pending();
    } else {
        s_accum = s_current;
    }

    s_pending_op = op;
    s_new_entry = true;
}

/* Handles equals (=) */
static void calc_handle_equals(void) {
    if (s_error) return;

    if (s_pending_op != 0) {
        execute_pending();
        s_pending_op = 0;
        s_new_entry = true;
    }
}

/* Handles Clear All (C) */
static void calc_handle_clear(void) {
    s_accum = 0;
    s_current = 0;
    s_pending_op = 0;
    s_new_entry = true;
    s_error = false;
    strcpy(s_display_str, "0");
    update_display();
}

/* Handles Clear Entry (CE) */
static void calc_handle_clear_entry(void) {
    s_current = 0;
    s_new_entry = true;
    s_error = false;
    strcpy(s_display_str, "0");
    update_display();
}

/* Handles Negate (+/-) */
static void calc_handle_negate(void) {
    if (s_error || s_current == 0) return;
    s_current = -s_current;
    format_display(s_current);
    update_display();
}

/* Handles Backspace */
static void calc_handle_backspace(void) {
    if (s_error || s_new_entry) return;
    s_current /= 10;
    format_display(s_current);
    update_display();
}

/* Button Callbacks */
static void on_btn_digit(widget_t* w, void* data) {
    (void)w;
    int d = (int)(intptr_t)data;
    calc_handle_digit(d);
}

static void on_btn_op(widget_t* w, void* data) {
    (void)w;
    char op = (char)(intptr_t)data;
    calc_handle_op(op);
}

static void on_btn_eq(widget_t* w, void* data) {
    (void)w; (void)data;
    calc_handle_equals();
}

static void on_btn_c(widget_t* w, void* data) {
    (void)w; (void)data;
    calc_handle_clear();
}

static void on_btn_ce(widget_t* w, void* data) {
    (void)w; (void)data;
    calc_handle_clear_entry();
}

static void on_btn_neg(widget_t* w, void* data) {
    (void)w; (void)data;
    calc_handle_negate();
}

/* Helper: Integer square root via Newton's method */
static int32_t isqrt(int32_t n) {
    if (n <= 0) return 0;
    int32_t x0 = n / 2;
    if (x0 == 0) return 1;
    int32_t x1 = (x0 + n / x0) / 2;
    while (x1 < x0) {
        x0 = x1;
        x1 = (x0 + n / x0) / 2;
    }
    return x0;
}

/* Button: Square Root (sqrt) */
static void on_btn_sqrt(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_current < 0) {
        s_error = true;
        strcpy(s_display_str, "Invalid Input");
        update_display();
        return;
    }
    s_current = isqrt(s_current);
    format_display(s_current);
    update_display();
    s_new_entry = true;
}

/* Button: Square (x^2) */
static void on_btn_sqr(widget_t* w, void* data) {
    (void)w; (void)data;
    s_current = s_current * s_current;
    format_display(s_current);
    update_display();
    s_new_entry = true;
}

/* Button: Reciprocal (1/x) */
static void on_btn_recip(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_current == 0) {
        s_error = true;
        strcpy(s_display_str, "Error: /0");
        update_display();
        return;
    }
    s_current = (s_current == 1) ? 1 : 0;
    format_display(s_current);
    update_display();
    s_new_entry = true;
}

/* Button: Percentage (%) */
static void on_btn_pct(widget_t* w, void* data) {
    (void)w; (void)data;
    s_current = (s_accum * s_current) / 100;
    format_display(s_current);
    update_display();
    s_new_entry = true;
}

/* Button: Backspace (DEL) */
static void on_btn_del(widget_t* w, void* data) {
    (void)w; (void)data;
    if (s_error || s_new_entry) return;
    s_current /= 10;
    format_display(s_current);
    update_display();
}

/* Window Key Event Dispatcher */
static void on_calc_key(window_t* win, char c) {
    (void)win;
    if (c >= '0' && c <= '9') {
        calc_handle_digit(c - '0');
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_handle_op(c);
    } else if (c == '=' || c == '\n' || c == '\r') {
        calc_handle_equals();
    } else if (c == 'c' || c == 'C') {
        calc_handle_clear();
    } else if (c == '\b') {
        calc_handle_backspace();
    }
}

/* Window Close Handler */
static void on_calc_close(window_t* win) {
    wm_remove_window(win);
    s_calc_win = NULL;
    s_lbl_display = NULL;
}

/*
 * app_calc_open:
 * Creates and displays the Calculator application window.
 */
void app_calc_open(void) {
    if (s_calc_win) {
        wm_bring_to_front(s_calc_win);
        return;
    }

    s_calc_win = window_create(120, 90, 320, 330, "Scientific Calculator",
                               WINDOW_FLAG_VISIBLE | WINDOW_FLAG_MOVABLE);
    if (!s_calc_win) return;

    s_calc_win->on_close = on_calc_close;
    s_calc_win->on_key   = on_calc_key;

    /* Reset calculation state */
    calc_handle_clear();

    /* 1. Digital Readout Display Box (Sunken border frame) */
    window_fill_rect(s_calc_win, 12, 12, 286, 38, 0x00F0F8F4);
    window_draw_rect(s_calc_win, 12, 12, 286, 38, 0x00607060);

    /* Display Label inside readout box */
    s_lbl_display = widget_create_label(20, 22, 270, 20, "0", 0x00102010);
    widget_add_to_window(s_calc_win, s_lbl_display);

    /* 2. Keypad Buttons (5 columns x 5 rows) */
    /* Row 0 (y = 62): [DEL], [C], [CE], [+/-], [/] */
    widget_add_to_window(s_calc_win, widget_create_button(14,  62, 50, 36, "DEL", on_btn_del,  NULL));
    widget_add_to_window(s_calc_win, widget_create_button(72,  62, 50, 36, "C",   on_btn_c,    NULL));
    widget_add_to_window(s_calc_win, widget_create_button(130, 62, 50, 36, "CE",  on_btn_ce,   NULL));
    widget_add_to_window(s_calc_win, widget_create_button(188, 62, 50, 36, "+/-", on_btn_neg,  NULL));
    widget_add_to_window(s_calc_win, widget_create_button(246, 62, 50, 36, "/",   on_btn_op,   (void*)(intptr_t)'/'));

    /* Row 1 (y = 106): [sqrt], [7], [8], [9], [*] */
    widget_add_to_window(s_calc_win, widget_create_button(14,  106, 50, 36, "sqrt",on_btn_sqrt, NULL));
    widget_add_to_window(s_calc_win, widget_create_button(72,  106, 50, 36, "7",   on_btn_digit,(void*)(intptr_t)7));
    widget_add_to_window(s_calc_win, widget_create_button(130, 106, 50, 36, "8",   on_btn_digit,(void*)(intptr_t)8));
    widget_add_to_window(s_calc_win, widget_create_button(188, 106, 50, 36, "9",   on_btn_digit,(void*)(intptr_t)9));
    widget_add_to_window(s_calc_win, widget_create_button(246, 106, 50, 36, "*",   on_btn_op,   (void*)(intptr_t)'*'));

    /* Row 2 (y = 150): [x^2], [4], [5], [6], [-] */
    widget_add_to_window(s_calc_win, widget_create_button(14,  150, 50, 36, "x^2", on_btn_sqr,  NULL));
    widget_add_to_window(s_calc_win, widget_create_button(72,  150, 50, 36, "4",   on_btn_digit,(void*)(intptr_t)4));
    widget_add_to_window(s_calc_win, widget_create_button(130, 150, 50, 36, "5",   on_btn_digit,(void*)(intptr_t)5));
    widget_add_to_window(s_calc_win, widget_create_button(188, 150, 50, 36, "6",   on_btn_digit,(void*)(intptr_t)6));
    widget_add_to_window(s_calc_win, widget_create_button(246, 150, 50, 36, "-",   on_btn_op,   (void*)(intptr_t)'-'));

    /* Row 3 (y = 194): [1/x], [1], [2], [3], [+] */
    widget_add_to_window(s_calc_win, widget_create_button(14,  194, 50, 36, "1/x", on_btn_recip,NULL));
    widget_add_to_window(s_calc_win, widget_create_button(72,  194, 50, 36, "1",   on_btn_digit,(void*)(intptr_t)1));
    widget_add_to_window(s_calc_win, widget_create_button(130, 194, 50, 36, "2",   on_btn_digit,(void*)(intptr_t)2));
    widget_add_to_window(s_calc_win, widget_create_button(188, 194, 50, 36, "3",   on_btn_digit,(void*)(intptr_t)3));
    widget_add_to_window(s_calc_win, widget_create_button(246, 194, 50, 36, "+",   on_btn_op,   (void*)(intptr_t)'+'));

    /* Row 4 (y = 238): [%], [0] (wide), [.], [=] */
    widget_add_to_window(s_calc_win, widget_create_button(14,  238, 50,  36, "%",   on_btn_pct,  NULL));
    widget_add_to_window(s_calc_win, widget_create_button(72,  238, 108, 36, "0",   on_btn_digit,(void*)(intptr_t)0));
    widget_add_to_window(s_calc_win, widget_create_button(188, 238, 50,  36, ".",   on_btn_ce,   NULL));
    widget_add_to_window(s_calc_win, widget_create_button(246, 238, 50,  36, "=",   on_btn_eq,   NULL));

    wm_add_window(s_calc_win);
    serial_writestring("[CALC] Scientific Calculator window opened.\n");
}
