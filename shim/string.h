#ifndef HL_SHIM_STRING_H
#define HL_SHIM_STRING_H

#include "hl_c.h"

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);

#define strncmp hl_c_strncmp
#define strcmp hl_c_strcmp
#define strlen hl_c_strlen
#define strncpy hl_c_strncpy
#define strchr hl_c_strchr
#define memchr hl_c_memchr

#endif
