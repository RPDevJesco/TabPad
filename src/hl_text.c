#include "hl_int.h"

#define SLACK 64u

static int grow(struct hl_buf *b, uint32_t need)
{
    uint32_t cap;
    uint32_t tail;
    uint32_t i;
    uint16_t *p;

    if (need <= b->cap)
    {
        return 0;
    }

    cap = need + need / 2u + SLACK;
    p = hl_mem_alloc((size_t)cap * sizeof *p);

    if (!p)
    {
        return -1;
    }

    tail = b->len - b->gap;

    for (i = 0u; i < b->gap; i++)
    {
        p[i] = b->p[i];
    }

    for (i = 0u; i < tail; i++)
    {
        p[cap - tail + i] = b->p[b->cap - tail + i];
    }

    hl_mem_free(b->p);
    b->p = p;
    b->cap = cap;

    return 0;
}

static void gap_to(struct hl_buf *b, uint32_t at)
{
    uint32_t hole = b->cap - b->len;
    uint32_t i;

    for (i = b->gap; i > at; i--)
    {
        b->p[i - 1u + hole] = b->p[i - 1u];
    }

    for (i = b->gap; i < at; i++)
    {
        b->p[i] = b->p[i + hole];
    }

    b->gap = at;
}

int hl_buf_open(struct hl_buf *b, const uint16_t *text, uint32_t len)
{
    uint32_t i;

    b->p = 0;
    b->cap = 0u;
    b->len = 0u;
    b->gap = 0u;

    if (grow(b, len + 1u))
    {
        return -1;
    }

    for (i = 0u; i < len; i++)
    {
        b->p[i] = text[i];
    }

    b->len = len;
    b->gap = len;

    return 0;
}

void hl_buf_close(struct hl_buf *b)
{
    hl_mem_free(b->p);
    b->p = 0;
    b->cap = 0u;
    b->len = 0u;
    b->gap = 0u;
}

int hl_buf_edit(struct hl_buf *b, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    uint32_t i;

    if (grow(b, b->len - removed + n + 1u))
    {
        return -1;
    }

    gap_to(b, at);

    b->len -= removed;

    for (i = 0u; i < n; i++)
    {
        b->p[at + i] = ins[i];
    }

    b->gap = at + n;
    b->len += n;

    return 0;
}

const uint16_t *hl_buf_run(const struct hl_buf *b, uint32_t off, uint32_t *n)
{
    if (off < b->gap)
    {
        *n = b->gap - off;

        return b->p + off;
    }

    if (off < b->len)
    {
        *n = b->len - off;

        return b->p + off + (b->cap - b->len);
    }

    *n = 0u;

    return b->p;
}
