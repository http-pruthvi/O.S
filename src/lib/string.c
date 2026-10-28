#include "lib/string.h"

/*
 * strlen:
 * Computes the number of characters in a null-terminated string,
 * excluding the terminating null byte ('\0').
 */
size_t strlen(const char* str) {
    size_t len = 0;
    if (!str) {
        return 0;
    }
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

/*
 * memset:
 * Fills the first count bytes of the memory area pointed to by dest
 * with the constant byte val.
 */
void* memset(void* dest, int val, size_t count) {
    uint8_t* ptr = (uint8_t*)dest;
    uint8_t byte_val = (uint8_t)val;
    for (size_t i = 0; i < count; i++) {
        ptr[i] = byte_val;
    }
    return dest;
}

/*
 * memcpy:
 * Copies count bytes from memory area src to memory area dest.
 * The memory areas must not overlap.
 */
void* memcpy(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

/*
 * memcmp:
 * Compares the first count bytes of memory area s1 and memory area s2.
 * Returns:
 *   < 0 if s1 is less than s2
 *     0 if s1 is equal to s2
 *   > 0 if s1 is greater than s2
 */
int memcmp(const void* s1, const void* s2, size_t count) {
    const uint8_t* p1 = (const uint8_t*)s1;
    const uint8_t* p2 = (const uint8_t*)s2;
    for (size_t i = 0; i < count; i++) {
        if (p1[i] != p2[i]) {
            return (int)p1[i] - (int)p2[i];
        }
    }
    return 0;
}
