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
 * memmove:
 * Copies count bytes between potentially overlapping memory areas.
 */
void* memmove(void* dest, const void* src, size_t count) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        /* Copy forward when dest is below src */
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        /* Copy backward when dest is above src to prevent overwriting */
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
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

/*
 * strcpy:
 * Copies the string pointed to by src, including the terminating null byte,
 * to the buffer pointed to by dest.
 */
char* strcpy(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while ((*d++ = *src++) != '\0') {}
    return dest;
}

/*
 * strncpy:
 * Copies at most count bytes from src to dest. If src is less than count bytes,
 * remaining bytes in dest are filled with null bytes.
 */
char* strncpy(char* dest, const char* src, size_t count) {
    if (!dest || !src) return dest;
    size_t i = 0;
    for (i = 0; i < count && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < count; i++) {
        dest[i] = '\0';
    }
    return dest;
}

/*
 * strcmp:
 * Compares two null-terminated strings s1 and s2 lexicographically.
 */
int strcmp(const char* s1, const char* s2) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/*
 * strncmp:
 * Compares at most count characters of two strings s1 and s2.
 */
int strncmp(const char* s1, const char* s2, size_t count) {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;

    for (size_t i = 0; i < count; i++) {
        if (s1[i] != s2[i]) {
            return (int)(unsigned char)s1[i] - (int)(unsigned char)s2[i];
        }
        if (s1[i] == '\0') {
            break;
        }
    }
    return 0;
}

/*
 * strcat:
 * Appends the src string to the dest string, overwriting the terminating null byte
 * and then adds a terminating null byte.
 */
char* strcat(char* dest, const char* src) {
    if (!dest || !src) return dest;
    char* d = dest;
    while (*d != '\0') {
        d++;
    }
    while ((*d++ = *src++) != '\0') {}
    return dest;
}

/*
 * strncat:
 * Appends at most count characters from src to dest.
 */
char* strncat(char* dest, const char* src, size_t count) {
    if (!dest || !src || count == 0) return dest;
    char* d = dest;
    while (*d != '\0') {
        d++;
    }
    size_t i = 0;
    while (i < count && *src != '\0') {
        *d++ = *src++;
        i++;
    }
    *d = '\0';
    return dest;
}

/*
 * strlcpy:
 * Bounded string copy with guaranteed null-termination.
 * Always null-terminates dest if size > 0.
 * Returns total length of src string (for truncation detection).
 */
size_t strlcpy(char* dest, const char* src, size_t size) {
    if (!src) return 0;
    size_t src_len = strlen(src);

    if (size > 0 && dest) {
        size_t copy_len = (src_len >= size) ? (size - 1) : src_len;
        memcpy(dest, src, copy_len);
        dest[copy_len] = '\0';
    }

    return src_len;
}

/*
 * strlcat:
 * Bounded string concatenation with guaranteed null-termination.
 * Appends at most (size - dest_len - 1) characters from src to dest.
 * Returns initial length of dest + length of src.
 */
size_t strlcat(char* dest, const char* src, size_t size) {
    if (!dest || !src) return 0;
    size_t dest_len = strlen(dest);
    size_t src_len = strlen(src);

    if (dest_len >= size) {
        return size + src_len;
    }

    size_t copy_len = size - dest_len - 1;
    if (src_len < copy_len) {
        copy_len = src_len;
    }

    memcpy(dest + dest_len, src, copy_len);
    dest[dest_len + copy_len] = '\0';

    return dest_len + src_len;
}


