#ifndef LIB_TYPES_H
#define LIB_TYPES_H

/*
 * types.h:
 * Project-local fixed-width integer types and basic definitions.
 * Freestanding C kernels do not link against the standard C library (libc),
 * so we explicitly define fixed-width types conforming to x86 32-bit ILP32.
 */

/* Unsigned fixed-width integer types */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Signed fixed-width integer types */
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* Types for memory sizes and pointer arithmetic */
typedef uint32_t           size_t;
typedef int32_t            ssize_t;
typedef uint32_t           uintptr_t;
typedef int32_t            intptr_t;

/* Standard NULL pointer */
#define NULL ((void*)0)

/* Boolean values */
typedef enum {
    false = 0,
    true  = 1
} bool;

#endif /* LIB_TYPES_H */
