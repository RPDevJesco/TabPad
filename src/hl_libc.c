#include "hl_c.h"
#include "hl.h"

#define HEAD 16u

void *hl_c_malloc(size_t n)
{
    uint8_t *p;

    if (n > SIZE_MAX - HEAD)
    {
        return 0;
    }

    p = hl_mem_alloc(n + HEAD);

    if (!p)
    {
        return 0;
    }

    *(size_t *)(void *)p = n;

    return p + HEAD;
}

void *hl_c_calloc(size_t count, size_t n)
{
    uint8_t *p;
    size_t i;

    if (n && count > (SIZE_MAX - HEAD) / n)
    {
        return 0;
    }

    p = hl_c_malloc(count * n);

    if (!p)
    {
        return 0;
    }

    for (i = 0u; i < count * n; i++)
    {
        p[i] = 0u;
    }

    return p;
}

void hl_c_free(void *p)
{
    if (!p)
    {
        return;
    }

    hl_mem_free((uint8_t *)p - HEAD);
}

void *hl_c_realloc(void *p, size_t n)
{
    const uint8_t *old = p;
    uint8_t *q;
    size_t keep;
    size_t i;

    q = hl_c_malloc(n);

    if (!q || !p)
    {
        return q;
    }

    keep = *(const size_t *)(const void *)(old - HEAD);
    keep = keep < n ? keep : n;

    for (i = 0u; i < keep; i++)
    {
        q[i] = old[i];
    }

    hl_c_free(p);

    return q;
}

void hl_c_abort(void)
{
    hl_panic();

    for (;;)
    {
    }
}

int hl_c_strncmp(const char *a, const char *b, size_t n)
{
    size_t i;

    for (i = 0u; i < n; i++)
    {
        if (a[i] != b[i])
        {
            return (uint8_t)a[i] < (uint8_t)b[i] ? -1 : 1;
        }

        if (!a[i])
        {
            return 0;
        }
    }

    return 0;
}

int hl_c_strcmp(const char *a, const char *b)
{
    size_t i;

    for (i = 0u; a[i] && a[i] == b[i]; i++)
    {
    }

    if (a[i] == b[i])
    {
        return 0;
    }

    return (uint8_t)a[i] < (uint8_t)b[i] ? -1 : 1;
}

size_t hl_c_strlen(const char *s)
{
    size_t n = 0u;

    while (s[n])
    {
        n++;
    }

    return n;
}

char *hl_c_strncpy(char *dst, const char *src, size_t n)
{
    size_t i;

    for (i = 0u; i < n && src[i]; i++)
    {
        dst[i] = src[i];
    }

    for (; i < n; i++)
    {
        dst[i] = 0;
    }

    return dst;
}

char *hl_c_strchr(const char *s, int c)
{
    size_t i;

    for (i = 0u; s[i] != (char)c; i++)
    {
        if (!s[i])
        {
            return 0;
        }
    }

    return (char *)(uintptr_t)(s + i);
}

void *hl_c_memchr(const void *s, int c, size_t n)
{
    const uint8_t *p = s;
    size_t i;

    for (i = 0u; i < n; i++)
    {
        if (p[i] == (uint8_t)c)
        {
            return (void *)(uintptr_t)(p + i);
        }
    }

    return 0;
}

int hl_c_isprint(int c)
{
    return c >= 0x20 && c < 0x7F;
}

int hl_c_isspace(uint32_t c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

int hl_c_isupper(uint32_t c)
{
    return c >= 'A' && c <= 'Z';
}

int hl_c_islower(uint32_t c)
{
    return c >= 'a' && c <= 'z';
}

int hl_c_isalpha(uint32_t c)
{
    return hl_c_isupper(c) || hl_c_islower(c);
}

int hl_c_isdigit(uint32_t c)
{
    return c >= '0' && c <= '9';
}

int hl_c_isalnum(uint32_t c)
{
    return hl_c_isalpha(c) || hl_c_isdigit(c);
}

int hl_c_isxdigit(uint32_t c)
{
    return hl_c_isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

int hl_c_ispunct(uint32_t c)
{
    return c > 0x20u && c < 0x7Fu && !hl_c_isalnum(c);
}

uint32_t hl_c_toupper(uint32_t c)
{
    return hl_c_islower(c) ? c - 32u : c;
}

uint32_t hl_c_tolower(uint32_t c)
{
    return hl_c_isupper(c) ? c + 32u : c;
}

int hl_c_vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    (void)fmt;
    (void)ap;

    if (buf && n)
    {
        buf[0] = 0;
    }

    return 0;
}

int hl_c_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    (void)fmt;

    if (buf && n)
    {
        buf[0] = 0;
    }

    return 0;
}

int hl_c_fprintf(FILE *f, const char *fmt, ...)
{
    (void)f;
    (void)fmt;

    return 0;
}

int hl_c_fputc(int c, FILE *f)
{
    (void)f;

    return c;
}

int hl_c_fputs(const char *s, FILE *f)
{
    (void)s;
    (void)f;

    return 0;
}

int hl_c_fclose(FILE *f)
{
    (void)f;

    return 0;
}

FILE *hl_c_fdopen(int fd, const char *mode)
{
    (void)fd;
    (void)mode;

    return 0;
}
