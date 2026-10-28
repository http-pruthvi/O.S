#ifndef LIB_KPRINTF_H
#define LIB_KPRINTF_H

#include "types.h"

/* Freestanding variadic argument macros provided by GCC built-ins */
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

/*
 * kprintf:
 * Kernel formatted print function.
 * Formats strings and writes them simultaneously to BOTH VGA text screen and COM1 serial.
 *
 * Supported format specifiers:
 *   %c - Character
 *   %s - String
 *   %d - Signed decimal 32-bit integer
 *   %u - Unsigned decimal 32-bit integer
 *   %x - Hexadecimal integer (lowercase, e.g. 0x1a2b)
 *   %p - Pointer address (e.g. 0x00100000)
 *   %% - Literal percent sign
 */
int kprintf(const char* fmt, ...);

/* Variadic list version of kprintf */
int vkprintf(const char* fmt, va_list args);

#endif /* LIB_KPRINTF_H */
