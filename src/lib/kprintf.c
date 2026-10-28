#include "lib/kprintf.h"
#include "drivers/vga.h"
#include "drivers/serial.h"

/*
 * kputc:
 * Internal helper to write a character to all enabled kernel output sinks.
 * In Milestone 1, this writes simultaneously to VGA text mode and COM1 serial.
 */
static void kputc(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

/*
 * print_unsigned:
 * Converts an unsigned 32-bit integer into a string representation in the given base
 * (base 10 for decimal, base 16 for hexadecimal) and prints it.
 */
static int print_unsigned(uint32_t num, uint32_t base, bool pad_zeros, int min_width) {
    char buf[32];
    int count = 0;
    int digits = 0;
    static const char hex_chars[] = "0123456789abcdef";

    if (num == 0) {
        buf[digits++] = '0';
    } else {
        while (num > 0) {
            buf[digits++] = hex_chars[num % base];
            num /= base;
        }
    }

    /* Apply padding if requested */
    int pad_needed = min_width - digits;
    while (pad_needed > 0) {
        kputc(pad_zeros ? '0' : ' ');
        count++;
        pad_needed--;
    }

    /* Characters were generated in reverse (least significant digit first) */
    for (int i = digits - 1; i >= 0; i--) {
        kputc(buf[i]);
        count++;
    }

    return count;
}

/*
 * print_signed:
 * Converts a signed 32-bit integer to decimal string and outputs it.
 * Handles INT32_MIN (-2147483648) safely using unsigned arithmetic.
 */
static int print_signed(int32_t num) {
    int count = 0;
    uint32_t uval;

    if (num < 0) {
        kputc('-');
        count++;
        uval = (uint32_t)(-(num + 1)) + 1; /* Avoids overflow on -2147483648 */
    } else {
        uval = (uint32_t)num;
    }

    count += print_unsigned(uval, 10, false, 0);
    return count;
}

/*
 * vkprintf:
 * Core formatted printer processing format specifiers.
 */
int vkprintf(const char* fmt, va_list args) {
    if (!fmt) {
        return 0;
    }

    int count = 0;

    for (size_t i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            kputc(fmt[i]);
            count++;
            continue;
        }

        /* Format specifier encountered */
        i++;
        char spec = fmt[i];

        switch (spec) {
            case 'c': {
                char ch = (char)va_arg(args, int);
                kputc(ch);
                count++;
                break;
            }
            case 's': {
                const char* str = va_arg(args, const char*);
                if (!str) {
                    str = "(null)";
                }
                while (*str) {
                    kputc(*str++);
                    count++;
                }
                break;
            }
            case 'd': {
                int32_t val = va_arg(args, int32_t);
                count += print_signed(val);
                break;
            }
            case 'u': {
                uint32_t val = va_arg(args, uint32_t);
                count += print_unsigned(val, 10, false, 0);
                break;
            }
            case 'x': {
                uint32_t val = va_arg(args, uint32_t);
                count += print_unsigned(val, 16, false, 0);
                break;
            }
            case 'p': {
                uintptr_t val = (uintptr_t)va_arg(args, void*);
                kputc('0');
                kputc('x');
                count += 2;
                count += print_unsigned((uint32_t)val, 16, true, 8);
                break;
            }
            case '%': {
                kputc('%');
                count++;
                break;
            }
            default: {
                /* Unknown specifier: print literally */
                kputc('%');
                kputc(spec);
                count += 2;
                break;
            }
        }
    }

    return count;
}

/*
 * kprintf:
 * Variadic wrapper around vkprintf.
 */
int kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int written = vkprintf(fmt, args);
    va_end(args);
    return written;
}
