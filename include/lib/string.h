#ifndef LIB_STRING_H
#define LIB_STRING_H

#include "types.h"

/*
 * string.h:
 * Freestanding memory and string manipulation utilities.
 * Implemented without libc dependencies.
 */

/* Computes the length of a null-terminated string */
size_t strlen(const char* str);

/* Fills a block of memory with a specific byte value */
void* memset(void* dest, int val, size_t count);

/* Copies count bytes from src memory region to dest memory region */
void* memcpy(void* dest, const void* src, size_t count);

/* Copies count bytes between potentially overlapping memory regions */
void* memmove(void* dest, const void* src, size_t count);

/* Compares two blocks of memory byte by byte */
int memcmp(const void* s1, const void* s2, size_t count);

#endif /* LIB_STRING_H */
