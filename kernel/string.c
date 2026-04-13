#include "string.h"
#include <stddef.h>
#include <stdint.h>

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n-- && (*d++ = *src++));
    while (n-- > 0) *d++ = '\0';
    return dst;
}

int strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && (*a == *b)) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++));
    return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *d = dst + strlen(dst);
    while (n-- && (*d++ = *src++));
    *d = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : 0;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return (char *)haystack;
        }
    }
    return 0;
}

int strcontains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != 0;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = s;
    while (n--) *p++ = (uint8_t)c;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb) return *pa - *pb;
        pa++; pb++;
    }
    return 0;
}

void itoa(int n, char *buf, int base) {
    if (base < 2 || base > 16) { buf[0] = '\0'; return; }
    char tmp[34];
    int neg = 0, i = 0;
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    if (n < 0 && base == 10) { neg = 1; n = -n; }
    unsigned int u = (unsigned int)n;
    while (u) {
        int r = u % base;
        tmp[i++] = r < 10 ? '0' + r : 'a' + r - 10;
        u /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}

int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

const char *str_skip(const char *s, size_t n) {
    while (n-- && *s) s++;
    return s;
}

int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a >= 'A' && *a <= 'Z' ? *a + 32 : *a;
        char cb = *b >= 'A' && *b <= 'Z' ? *b + 32 : *b;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int str_replace(const char *src, const char *old, const char *newstr,
                char *dst, size_t dstsz) {
    char *pos = strstr(src, old);
    if (!pos) {
        strncpy(dst, src, dstsz - 1);
        dst[dstsz - 1] = '\0';
        return 0;
    }
    size_t before = (size_t)(pos - src);
    size_t after   = strlen(pos + strlen(old));
    size_t total   = before + strlen(newstr) + after;
    if (total + 1 > dstsz) return -1;
    memcpy(dst, src, before);
    strcpy(dst + before, newstr);
    strcpy(dst + before + strlen(newstr), pos + strlen(old));
    return 0;
}
