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

/* Copies null-terminated string from src to dest, returns dest */
char* strcpy(char* dest, const char* src);

/* Copies at most count characters from src to dest */
char* strncpy(char* dest, const char* src, size_t count);

/* Compares two null-terminated strings */
int strcmp(const char* s1, const char* s2);

/* Compares at most count characters of two strings */
int strncmp(const char* s1, const char* s2, size_t count);

/* Appends src string to dest string */
char* strcat(char* dest, const char* src);

/* Appends at most count characters of src to dest */
char* strncat(char* dest, const char* src, size_t count);

/* Bounded string copy with guaranteed null-termination (Security Hardened) */
size_t strlcpy(char* dest, const char* src, size_t size);

/* Bounded string append with guaranteed null-termination (Security Hardened) */
size_t strlcat(char* dest, const char* src, size_t size);

#endif /* LIB_STRING_H */
