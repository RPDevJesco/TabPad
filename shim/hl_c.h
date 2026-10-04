#ifndef HL_C_H
#define HL_C_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

typedef struct hl_c_file FILE;

void *hl_c_malloc(size_t n);
void *hl_c_calloc(size_t count, size_t n);
void *hl_c_realloc(void *p, size_t n);
void hl_c_free(void *p);
void hl_c_abort(void);
int hl_c_strncmp(const char *a, const char *b, size_t n);
int hl_c_strcmp(const char *a, const char *b);
size_t hl_c_strlen(const char *s);
char *hl_c_strncpy(char *dst, const char *src, size_t n);
char *hl_c_strchr(const char *s, int c);
void *hl_c_memchr(const void *s, int c, size_t n);
int hl_c_isprint(int c);
int hl_c_isspace(uint32_t c);
int hl_c_isalpha(uint32_t c);
int hl_c_isdigit(uint32_t c);
int hl_c_isalnum(uint32_t c);
int hl_c_isxdigit(uint32_t c);
int hl_c_isupper(uint32_t c);
int hl_c_islower(uint32_t c);
int hl_c_ispunct(uint32_t c);
uint32_t hl_c_toupper(uint32_t c);
uint32_t hl_c_tolower(uint32_t c);
int hl_c_fprintf(FILE *f, const char *fmt, ...);
int hl_c_snprintf(char *buf, size_t n, const char *fmt, ...);
int hl_c_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int hl_c_fputc(int c, FILE *f);
int hl_c_fputs(const char *s, FILE *f);
int hl_c_fclose(FILE *f);
FILE *hl_c_fdopen(int fd, const char *mode);

#endif
