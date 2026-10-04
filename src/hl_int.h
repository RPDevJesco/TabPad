#ifndef HL_INT_H
#define HL_INT_H

#include "hl.h"

#define HL_NONE 0xFFFFFFFFu

struct TSQuery;
struct TSQueryMatch;
struct hl_buf {
    uint16_t *p;
    uint32_t cap, len, gap;
};
struct hl_ql {
    struct TSQuery *q;
    uint32_t *theme;
    char **lua;
    struct TSQuery *inner[HL_INNER_MAX];
    uint32_t inner_i[HL_INNER_MAX];
    uint32_t inner_n[HL_INNER_MAX];
    int state;
};
const struct hl_ql *hl_q_get(uint32_t lang);

int hl_buf_open(struct hl_buf *b, const uint16_t *text, uint32_t len);
void hl_buf_close(struct hl_buf *b);
int hl_buf_edit(struct hl_buf *b, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n);
const uint16_t *hl_buf_run(const struct hl_buf *b, uint32_t off, uint32_t *n);
int hl_re_ok(const char *pat, uint32_t n);
int hl_re_match(const char *pat, uint32_t n, const struct hl_buf *b, uint32_t at, uint32_t len);
int hl_re_find(const char *pat, uint32_t n, const struct hl_buf *b, uint32_t from, uint32_t to, uint32_t *at, uint32_t *len, uint32_t *groups);
int hl_q_pass(const struct hl_ql *l, const struct hl_buf *b, const struct TSQueryMatch *m);

static inline uint16_t hl_buf_at(const struct hl_buf *b, uint32_t i)
{
    return b->p[i < b->gap ? i : i + (b->cap - b->len)];
}

static inline uint32_t hl_buf_cp(const struct hl_buf *b, uint32_t i, uint32_t end, uint32_t *w)
{
    uint32_t hi = hl_buf_at(b, i);
    uint32_t lo;

    *w = 1u;

    if (hi < 0xD800u || hi > 0xDBFFu || i + 1u >= end)
    {
        return hi;
    }

    lo = hl_buf_at(b, i + 1u);

    if (lo < 0xDC00u || lo > 0xDFFFu)
    {
        return hi;
    }

    *w = 2u;

    return 0x10000u + ((hi - 0xD800u) << 10) + (lo - 0xDC00u);
}

static inline uint32_t hl_utf8(const char **p, const char *e)
{
    const uint8_t *s = (const uint8_t *)*p;
    uint32_t c = s[0];
    uint32_t more = 0u;
    uint32_t i;

    if (c >= 0xF0u)
    {
        more = 3u;
        c &= 0x07u;
    }

    else if (c >= 0xE0u)
    {
        more = 2u;
        c &= 0x0Fu;
    }

    else if (c >= 0xC0u)
    {
        more = 1u;
        c &= 0x1Fu;
    }

    else if (c >= 0x80u)
    {
        *p += 1;

        return 0xFFFDu;
    }

    if ((uint32_t)(e - *p) <= more)
    {
        *p += 1;

        return 0xFFFDu;
    }

    for (i = 1u; i <= more; i++)
    {
        if ((s[i] & 0xC0u) != 0x80u)
        {
            *p += 1;

            return 0xFFFDu;
        }

        c = c << 6 | (s[i] & 0x3Fu);
    }

    *p += more + 1u;

    return c;
}

#endif
