#include "hl_int.h"

#define MANY 0xFFFFFFFFu
#define COUNT_MAX 1000u
#define DEPTH_MAX 200u
#define NEST_MAX 16u
#define STEPS_FLAT 20000u
#define STEPS_PER_UNIT 64u

struct re {
    const struct hl_buf *b;
    uint32_t start, end;
    uint32_t steps, depth;
    uint32_t stop;
    int icase, lines;
    const char *pat;
    uint32_t *caps;
};

struct cont {
    const char *p, *e;
    const char *ap, *ae;
    uint32_t min, max, n, from;
    int shut;
    const struct cont *next;
};

static int is_punct(uint32_t c)
{
    return (c >= 0x21u && c <= 0x2Fu) || (c >= 0x3Au && c <= 0x40u) || (c >= 0x5Bu && c <= 0x60u) || (c >= 0x7Bu && c <= 0x7Eu);
}

static int is_digit(uint32_t c)
{
    return c >= '0' && c <= '9';
}

static int is_word(uint32_t c)
{
    return is_digit(c) || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c > 0x7Fu;
}

static int is_space(uint32_t c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}

static uint32_t lower(uint32_t c)
{
    return c >= 'A' && c <= 'Z' ? c + 32u : c;
}

static uint32_t upper(uint32_t c)
{
    return c >= 'a' && c <= 'z' ? c - 32u : c;
}

static int esc_set(char e, uint32_t c, int *hit)
{
    switch (e)
    {
        case 'd': *hit = is_digit(c); return 1;
        case 'D': *hit = !is_digit(c); return 1;
        case 'w': *hit = is_word(c); return 1;
        case 'W': *hit = !is_word(c); return 1;
        case 's': *hit = is_space(c); return 1;
        case 'S': *hit = !is_space(c); return 1;
        default: return 0;
    }
}

static uint32_t esc_char(char e)
{
    switch (e)
    {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        default: return (uint8_t)e;
    }
}

static int in_range(const struct re *r, uint32_t c, uint32_t lo, uint32_t hi)
{
    if (c >= lo && c <= hi)
    {
        return 1;
    }

    if (!r->icase)
    {
        return 0;
    }

    return (lower(c) >= lo && lower(c) <= hi) || (upper(c) >= lo && upper(c) <= hi);
}

static uint32_t class_char(const char **q, const char *e)
{
    if (**q == '\\')
    {
        *q += 2;

        return esc_char((*q)[-1]);
    }

    return hl_utf8(q, e);
}

static int class_has(const struct re *r, const char *p, const char *e, uint32_t c)
{
    const char *q = p + 1;
    uint32_t lo;
    uint32_t hi;
    int neg = 0;
    int hit = 0;
    int one;

    if (*q == '^')
    {
        neg = 1;
        q++;
    }

    while (*q != ']')
    {
        if (*q == '\\' && esc_set(q[1], c, &one))
        {
            hit |= one;
            q += 2;
            continue;
        }

        lo = class_char(&q, e);
        hi = lo;

        if (q[0] == '-' && q[1] != ']')
        {
            q++;
            hi = class_char(&q, e);
        }

        hit |= in_range(r, c, lo, hi);
    }

    return hit != neg;
}

static const char *atom_end(const char *p, const char *e)
{
    const char *q = p;
    uint32_t depth = 0u;

    if (*p == '\\')
    {
        return p + 2;
    }

    if (*p == '[')
    {
        q = p + 1;

        while (*q != ']')
        {
            q += *q == '\\' ? 2 : 1;
        }

        return q + 1;
    }

    if (*p != '(')
    {
        hl_utf8(&q, e);

        return q;
    }

    for (;;)
    {
        if (*q == '\\' || *q == '[')
        {
            q = atom_end(q, e);
            continue;
        }

        if (*q == '(')
        {
            depth++;
        }

        else if (*q == ')')
        {
            depth--;
        }

        q++;

        if (!depth)
        {
            return q;
        }
    }
}

static int count(const char **p, const char *e, uint32_t *v)
{
    const char *q = *p;
    uint32_t n = 0u;

    while (q < e && is_digit((uint8_t)*q) && n <= COUNT_MAX)
    {
        n = n * 10u + (uint32_t)(*q - '0');
        q++;
    }

    if (q == *p || n > COUNT_MAX)
    {
        return -1;
    }

    *p = q;
    *v = n;

    return 0;
}

static const char *quant(const char *p, const char *e, uint32_t *min, uint32_t *max)
{
    *min = 1u;
    *max = 1u;

    if (p == e)
    {
        return p;
    }

    if (*p == '*')
    {
        *min = 0u;
        *max = MANY;
        p++;
    }

    else if (*p == '+')
    {
        *max = MANY;
        p++;
    }

    else if (*p == '?')
    {
        *min = 0u;
        p++;
    }

    else if (*p == '{')
    {
        p++;

        if (count(&p, e, min))
        {
            return 0;
        }

        *max = *min;

        if (p < e && *p == ',')
        {
            p++;
            *max = MANY;

            if (p < e && *p != '}' && (count(&p, e, max) || *max < *min))
            {
                return 0;
            }
        }

        if (p == e || *p != '}')
        {
            return 0;
        }

        p++;
    }

    else
    {
        return p;
    }

    if (p < e && *p == '?')
    {
        p++;
    }

    return p;
}

static int is_anchor(const char *p)
{
    return *p == '^' || *p == '$' || (*p == '\\' && (p[1] == 'b' || p[1] == 'B'));
}

static const char *ok_alt(const char *p, const char *e, uint32_t nest);

static const char *ok_class_char(const char *p, const char *e)
{
    if (p == e || *p == '[' || *p == ']')
    {
        return 0;
    }

    if (*p != '\\')
    {
        hl_utf8(&p, e);

        return p;
    }

    if (p + 1 == e || !(is_punct((uint8_t)p[1]) || p[1] == 'n' || p[1] == 't' || p[1] == 'r'))
    {
        return 0;
    }

    return p + 2;
}

static const char *ok_class(const char *p, const char *e)
{
    int unused;

    p++;

    if (p < e && *p == '^')
    {
        p++;
    }

    if (p == e || *p == ']')
    {
        return 0;
    }

    while (p < e && *p != ']')
    {
        if (p + 1 < e && p[0] == '&' && p[1] == '&')
        {
            return 0;
        }

        if (p + 1 < e && *p == '\\' && esc_set(p[1], 0u, &unused))
        {
            p += 2;
            continue;
        }

        p = ok_class_char(p, e);

        if (!p)
        {
            return 0;
        }

        if (p + 1 < e && p[0] == '-' && p[1] != ']')
        {
            p = ok_class_char(p + 1, e);

            if (!p)
            {
                return 0;
            }
        }
    }

    return p == e ? 0 : p + 1;
}

static const char *ok_atom(const char *p, const char *e, uint32_t nest)
{
    int unused;

    switch (*p)
    {
        case '*':
        case '+':
        case '?':
        case '{':
        case ')':
            return 0;

        case '[':
            return ok_class(p, e);

        case '\\':
            if (p + 1 == e)
            {
                return 0;
            }

            if (esc_set(p[1], 0u, &unused) || is_punct((uint8_t)p[1]))
            {
                return p + 2;
            }

            return p[1] == 'b' || p[1] == 'B' || p[1] == 'n' || p[1] == 't' || p[1] == 'r' ? p + 2 : 0;

        case '(':
            p++;

            if (p < e && *p == '?')
            {
                if (p + 1 == e || p[1] != ':')
                {
                    return 0;
                }

                p += 2;
            }

            p = ok_alt(p, e, nest + 1u);

            return p && p < e ? p + 1 : 0;

        default:
            hl_utf8(&p, e);

            return p;
    }
}

static const char *ok_alt(const char *p, const char *e, uint32_t nest)
{
    const char *a;
    uint32_t min;
    uint32_t max;

    if (nest > NEST_MAX)
    {
        return 0;
    }

    while (p < e && *p != ')')
    {
        if (*p == '|')
        {
            p++;
            continue;
        }

        a = ok_atom(p, e, nest);

        if (!a)
        {
            return 0;
        }

        a = quant(a, e, &min, &max);

        if (!a || (is_anchor(p) && (min != 1u || max != 1u)) || (a < e && (*a == '*' || *a == '+' || *a == '{')))
        {
            return 0;
        }

        p = a;
    }

    return p;
}

static const char *skip_flags(const char *p, const char *e, int *icase)
{
    *icase = 0;

    if (e - p >= 4 && p[0] == '(' && p[1] == '?' && p[2] == 'i' && p[3] == ')')
    {
        *icase = 1;

        return p + 4;
    }

    return p;
}

int hl_re_ok(const char *pat, uint32_t n)
{
    const char *e = pat + n;
    int unused;

    return ok_alt(skip_flags(pat, e, &unused), e, 0u) == e;
}

static int alt(struct re *r, const char *p, const char *e, uint32_t s, const struct cont *k);
static int seq(struct re *r, const char *p, const char *e, uint32_t s, const struct cont *k);

static int accepts(const struct re *r, const char *p, const char *a, uint32_t c)
{
    uint32_t lit;
    int hit;

    if (*p == '.')
    {
        return c != '\n';
    }

    if (*p == '[')
    {
        return class_has(r, p, a, c);
    }

    if (*p == '\\')
    {
        return esc_set(p[1], c, &hit) ? hit : c == esc_char(p[1]);
    }

    lit = hl_utf8(&p, a);

    return in_range(r, c, lit, lit);
}

static int anchor_holds(const struct re *r, const char *p, uint32_t s)
{
    uint32_t w;
    int before;
    int after;

    if (*p == '^')
    {
        return s == r->start || (r->lines && hl_buf_at(r->b, s - 1u) == '\n');
    }

    if (*p == '$')
    {
        return s == r->end || (r->lines && hl_buf_at(r->b, s) == '\n');
    }

    before = s > r->start && is_word(hl_buf_at(r->b, s - 1u));
    after = s < r->end && is_word(hl_buf_cp(r->b, s, r->end, &w));

    return (before != after) == (p[1] == 'b');
}

static uint32_t number_of(const struct re *r, const char *p)
{
    const char *q = r->pat;
    uint32_t n = 0u;

    while (q <= p)
    {
        if (*q == '\\')
        {
            q += 2;
            continue;
        }

        if (*q == '[')
        {
            for (q++; *q != ']'; q += *q == '\\' ? 2 : 1)
            {
            }
        }

        n += *q == '(' && q[1] != '?' ? 1u : 0u;
        q++;
    }

    return n;
}

static int group(struct re *r, const char *p, const char *a, uint32_t s, const struct cont *k)
{
    struct cont shut;

    if (p[1] == '?')
    {
        return alt(r, p + 3, a - 1, s, k);
    }

    if (!r->caps)
    {
        return alt(r, p + 1, a - 1, s, k);
    }

    shut.ap = 0;
    shut.shut = 1;
    shut.n = number_of(r, p);
    shut.from = s;
    shut.next = k;

    return alt(r, p + 1, a - 1, s, &shut);
}

static int rep(struct re *r, const struct cont *f, uint32_t s)
{
    struct cont g;

    if (f->n < f->max)
    {
        g = *f;
        g.n = f->n + 1u;
        g.from = s;

        if (group(r, f->ap, f->ae, s, &g))
        {
            return 1;
        }
    }

    return f->n >= f->min && seq(r, f->p, f->e, s, f->next);
}

static int done(struct re *r, uint32_t s, const struct cont *k);

static int shut(struct re *r, uint32_t s, const struct cont *k)
{
    uint32_t from = 0u;
    uint32_t to = 0u;

    if (k->n >= HL_GROUPS)
    {
        return done(r, s, k->next);
    }

    from = r->caps[k->n * 2u];
    to = r->caps[k->n * 2u + 1u];
    r->caps[k->n * 2u] = k->from;
    r->caps[k->n * 2u + 1u] = s;

    if (done(r, s, k->next))
    {
        return 1;
    }

    r->caps[k->n * 2u] = from;
    r->caps[k->n * 2u + 1u] = to;

    return 0;
}

static int done(struct re *r, uint32_t s, const struct cont *k)
{
    if (!k)
    {
        r->stop = s;

        return 1;
    }

    if (k->shut)
    {
        return shut(r, s, k);
    }

    if (!k->ap)
    {
        return seq(r, k->p, k->e, s, k->next);
    }

    if (s == k->from)
    {
        return seq(r, k->p, k->e, s, k->next);
    }

    return rep(r, k, s);
}

static uint32_t back(const struct re *r, uint32_t s, uint32_t t)
{
    uint32_t lo = hl_buf_at(r->b, t - 1u);

    if (lo < 0xDC00u || lo > 0xDFFFu || t - s < 2u)
    {
        return 1u;
    }

    return (hl_buf_at(r->b, t - 2u) & 0xFC00u) == 0xD800u ? 2u : 1u;
}

static int run(struct re *r, const char *p, const char *a, uint32_t min, uint32_t max, uint32_t s, const struct cont *k)
{
    uint32_t t = s;
    uint32_t n = 0u;
    uint32_t w;

    while (n < max && t < r->end && r->steps && accepts(r, p, a, hl_buf_cp(r->b, t, r->end, &w)))
    {
        t += w;
        n++;
        r->steps--;
    }

    for (;;)
    {
        if (n < min)
        {
            return 0;
        }

        if (done(r, t, k))
        {
            return 1;
        }

        if (n == min)
        {
            return 0;
        }

        t -= back(r, s, t);
        n--;
    }
}

static int seq_from(struct re *r, const char *p, const char *e, uint32_t s, const struct cont *k)
{
    struct cont rest;
    const char *a;
    uint32_t min;
    uint32_t max;

    if (p == e)
    {
        return done(r, s, k);
    }

    a = atom_end(p, e);
    rest.p = quant(a, e, &min, &max);
    rest.e = e;
    rest.ap = 0;
    rest.shut = 0;
    rest.next = k;

    if (is_anchor(p))
    {
        return anchor_holds(r, p, s) && done(r, s, &rest);
    }

    if (*p != '(')
    {
        return run(r, p, a, min, max, s, &rest);
    }

    if (min == 1u && max == 1u)
    {
        return group(r, p, a, s, &rest);
    }

    rest.ap = p;
    rest.ae = a;
    rest.min = min;
    rest.max = max;
    rest.n = 0u;
    rest.from = s;

    return rep(r, &rest, s);
}

static int seq(struct re *r, const char *p, const char *e, uint32_t s, const struct cont *k)
{
    int hit;

    if (r->depth >= DEPTH_MAX || !r->steps)
    {
        return 0;
    }

    r->steps--;
    r->depth++;
    hit = seq_from(r, p, e, s, k);
    r->depth--;

    return hit;
}

static int alt(struct re *r, const char *p, const char *e, uint32_t s, const struct cont *k)
{
    const char *from = p;

    while (p < e)
    {
        if (*p != '|')
        {
            p = atom_end(p, e);
            continue;
        }

        if (seq(r, from, p, s, k))
        {
            return 1;
        }

        p++;
        from = p;
    }

    return seq(r, from, e, s, k);
}

static int pinned(const char *p, const char *e)
{
    if (p == e || *p != '^')
    {
        return 0;
    }

    while (p < e)
    {
        if (*p == '|')
        {
            return 0;
        }

        p = atom_end(p, e);
    }

    return 1;
}

int hl_re_match(const char *pat, uint32_t n, const struct hl_buf *b, uint32_t at, uint32_t len)
{
    const char *e = pat + n;
    struct re r;
    uint32_t s;
    uint32_t w;

    pat = skip_flags(pat, e, &r.icase);
    r.b = b;
    r.start = at;
    r.end = at + len;
    r.depth = 0u;
    r.lines = 0;
    r.caps = 0;
    r.stop = at;
    r.steps = len > (MANY - STEPS_FLAT) / STEPS_PER_UNIT ? MANY : STEPS_FLAT + len * STEPS_PER_UNIT;

    if (pinned(pat, e))
    {
        return alt(&r, pat, e, at, 0);
    }

    for (s = at; s <= r.end; s += w)
    {
        if (alt(&r, pat, e, s, 0))
        {
            return 1;
        }

        w = 1u;

        if (s < r.end)
        {
            hl_buf_cp(b, s, r.end, &w);
        }
    }

    return 0;
}

static uint32_t lead(const char *p, const char *e)
{
    const char *q = p;
    const char *a;
    uint32_t min;
    uint32_t max;
    uint32_t c;

    if (p == e || *p == '.' || *p == '[' || *p == '(' || *p == '\\' || *p == '^' || *p == '$' || *p == '|')
    {
        return MANY;
    }

    while (q < e)
    {
        if (*q == '|')
        {
            return MANY;
        }

        q = atom_end(q, e);
    }

    a = p;
    c = hl_utf8(&a, e);
    quant(a, e, &min, &max);

    return min ? c : MANY;
}

int hl_re_find(const char *pat, uint32_t n, const struct hl_buf *b, uint32_t from, uint32_t to, uint32_t *at, uint32_t *len, uint32_t *groups)
{
    const char *e = pat + n;
    struct re r;
    uint32_t first;
    uint32_t c;
    uint32_t s;
    uint32_t w;
    uint32_t i;

    pat = skip_flags(pat, e, &r.icase);
    first = lead(pat, e);
    r.b = b;
    r.start = 0u;
    r.end = b->len;
    r.lines = 1;
    r.pat = pat;
    r.caps = groups;

    for (s = from; s < to && s <= r.end; s += w)
    {
        w = 1u;
        c = s < r.end ? hl_buf_cp(b, s, r.end, &w) : MANY;

        if (first != MANY && c != first && !(r.icase && lower(c) == lower(first)))
        {
            continue;
        }

        for (i = 0u; groups && i < HL_GROUPS * 2u; i++)
        {
            groups[i] = MANY;
        }

        r.depth = 0u;
        r.steps = STEPS_FLAT;
        r.stop = s;

        if (alt(&r, pat, e, s, 0) && r.stop > s)
        {
            *at = s;
            *len = r.stop - s;

            if (groups)
            {
                groups[0] = s;
                groups[1] = r.stop;
            }

            return 1;
        }
    }

    return 0;
}
