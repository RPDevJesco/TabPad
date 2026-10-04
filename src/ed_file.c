#include "ed_int.h"

struct eol {
    int after_cr;
    int known;
    uint32_t *kind;
};

static void note(struct eol *e, uint32_t kind)
{
    if (e->known)
    {
        return;
    }

    *e->kind = kind;
    e->known = 1;
}

static uint32_t put(uint16_t *out, uint32_t n, uint32_t unit, struct eol *e)
{
    if (e->after_cr)
    {
        e->after_cr = 0;

        if (unit == '\n')
        {
            note(e, ED_EOL_CRLF);

            return n;
        }

        note(e, ED_EOL_CR);
    }

    if (unit == '\r')
    {
        e->after_cr = 1;
        unit = '\n';
    }

    else if (unit == '\n')
    {
        note(e, ED_EOL_LF);
    }

    out[n] = (uint16_t)unit;

    return n + 1u;
}

static uint32_t utf8_at(const uint8_t *b, uint32_t n, uint32_t *i)
{
    uint32_t c = b[*i];
    uint32_t more;
    uint32_t low;
    uint32_t k;

    if (c < 0x80u)
    {
        *i += 1u;

        return c;
    }

    if (c >= 0xC2u && c <= 0xDFu)
    {
        more = 1u;
        low = 0x80u;
        c &= 0x1Fu;
    }

    else if (c >= 0xE0u && c <= 0xEFu)
    {
        more = 2u;
        low = 0x800u;
        c &= 0x0Fu;
    }

    else if (c >= 0xF0u && c <= 0xF4u)
    {
        more = 3u;
        low = 0x10000u;
        c &= 0x07u;
    }

    else
    {
        return ED_NONE;
    }

    if (n - *i <= more)
    {
        return ED_NONE;
    }

    for (k = 1u; k <= more; k++)
    {
        if ((b[*i + k] & 0xC0u) != 0x80u)
        {
            return ED_NONE;
        }

        c = c << 6 | (b[*i + k] & 0x3Fu);
    }

    if (c < low || (c >= 0xD800u && c <= 0xDFFFu) || c > 0x10FFFFu)
    {
        return ED_NONE;
    }

    *i += more + 1u;

    return c;
}

uint32_t ed_utf8_units(const char *utf8, uint32_t n, uint16_t *out, uint32_t max)
{
    const uint8_t *b = (const uint8_t *)utf8;
    uint32_t i = 0u;
    uint32_t k = 0u;
    uint32_t c;

    while (i < n && k + 2u <= max)
    {
        c = utf8_at(b, n, &i);

        if (c == ED_NONE)
        {
            c = 0xFFFDu;
            i++;
        }

        if (c >= 0x10000u)
        {
            out[k++] = (uint16_t)(0xD800u + ((c - 0x10000u) >> 10));
            c = 0xDC00u + ((c - 0x10000u) & 0x3FFu);
        }

        out[k++] = (uint16_t)c;
    }

    return k;
}

static int is_utf8(const uint8_t *b, uint32_t n)
{
    uint32_t i = 0u;

    while (i < n)
    {
        if (utf8_at(b, n, &i) == ED_NONE)
        {
            return 0;
        }
    }

    return 1;
}

int ed_decode(const uint8_t *bytes, uint32_t n, int detect, uint16_t **text, uint32_t *len, uint32_t *enc, uint32_t *eol)
{
    struct eol e;
    uint16_t *out;
    uint32_t kind = ED_ENC_UTF8;
    uint32_t i = 0u;
    uint32_t k = 0u;
    uint32_t c;

    if (detect && n >= 3u && bytes[0] == 0xEFu && bytes[1] == 0xBBu && bytes[2] == 0xBFu)
    {
        kind = ED_ENC_UTF8_BOM;
        i = 3u;
    }

    else if (detect && n >= 2u && bytes[0] == 0xFFu && bytes[1] == 0xFEu)
    {
        kind = ED_ENC_UTF16_LE;
        i = 2u;
    }

    else if (detect && n >= 2u && bytes[0] == 0xFEu && bytes[1] == 0xFFu)
    {
        kind = ED_ENC_UTF16_BE;
        i = 2u;
    }

    if (kind <= ED_ENC_UTF8_BOM && !is_utf8(bytes + i, n - i))
    {
        kind = detect ? ED_ENC_LATIN1 : kind;
    }

    out = ed_mem_alloc(((size_t)n + 1u) * sizeof *out);

    if (!out)
    {
        return -1;
    }

    e.after_cr = 0;
    e.known = 0;
    e.kind = eol;

    while (i < n)
    {
        if (kind == ED_ENC_UTF16_LE || kind == ED_ENC_UTF16_BE)
        {
            if (n - i < 2u)
            {
                break;
            }

            c = kind == ED_ENC_UTF16_LE ? bytes[i] | (uint32_t)bytes[i + 1u] << 8 : bytes[i + 1u] | (uint32_t)bytes[i] << 8;
            i += 2u;
        }

        else if (kind == ED_ENC_LATIN1)
        {
            c = bytes[i++];
        }

        else
        {
            c = utf8_at(bytes, n, &i);

            if (c == ED_NONE)
            {
                c = 0xFFFDu;
                i++;
            }
        }

        if (c >= 0x10000u)
        {
            k = put(out, k, 0xD800u + ((c - 0x10000u) >> 10), &e);
            c = 0xDC00u + ((c - 0x10000u) & 0x3FFu);
        }

        k = put(out, k, c, &e);
    }

    if (e.after_cr)
    {
        note(&e, ED_EOL_CR);
    }

    *text = out;
    *len = k;

    if (detect)
    {
        *enc = kind;
    }

    return 0;
}

struct sink {
    uint8_t *out;
    uint32_t n;
    uint32_t enc, eol;
};

static void emit(struct sink *s, uint32_t c)
{
    if (s->enc == ED_ENC_UTF16_LE || s->enc == ED_ENC_UTF16_BE)
    {
        if (c >= 0x10000u)
        {
            emit(s, 0xD800u + ((c - 0x10000u) >> 10));
            c = 0xDC00u + ((c - 0x10000u) & 0x3FFu);
        }

        s->out[s->n++] = (uint8_t)(s->enc == ED_ENC_UTF16_LE ? c : c >> 8);
        s->out[s->n++] = (uint8_t)(s->enc == ED_ENC_UTF16_LE ? c >> 8 : c);
        return;
    }

    if (s->enc == ED_ENC_LATIN1)
    {
        s->out[s->n++] = (uint8_t)(c < 0x100u ? c : '?');
        return;
    }

    if (c < 0x80u)
    {
        s->out[s->n++] = (uint8_t)c;
        return;
    }

    if (c < 0x800u)
    {
        s->out[s->n++] = (uint8_t)(0xC0u | c >> 6);
    }

    else if (c < 0x10000u)
    {
        s->out[s->n++] = (uint8_t)(0xE0u | c >> 12);
        s->out[s->n++] = (uint8_t)(0x80u | (c >> 6 & 0x3Fu));
    }

    else
    {
        s->out[s->n++] = (uint8_t)(0xF0u | c >> 18);
        s->out[s->n++] = (uint8_t)(0x80u | (c >> 12 & 0x3Fu));
        s->out[s->n++] = (uint8_t)(0x80u | (c >> 6 & 0x3Fu));
    }

    s->out[s->n++] = (uint8_t)(0x80u | (c & 0x3Fu));
}

int ed_encode(uint32_t doc, uint32_t from, uint32_t to, uint32_t enc, uint32_t eol, uint8_t **bytes, uint32_t *n)
{
    const uint16_t *run;
    struct sink s;
    uint32_t high = 0u;
    uint32_t off;
    uint32_t got;
    uint32_t i;
    uint32_t c;
    s.out = ed_mem_alloc(((size_t)(to - from) + 1u) * 4u);

    if (!s.out)
    {
        return -1;
    }

    s.n = 0u;
    s.enc = enc;
    s.eol = eol;

    if (enc == ED_ENC_UTF8_BOM)
    {
        s.out[s.n++] = 0xEFu;
        s.out[s.n++] = 0xBBu;
        s.out[s.n++] = 0xBFu;
    }

    if (enc == ED_ENC_UTF16_LE || enc == ED_ENC_UTF16_BE)
    {
        emit(&s, 0xFEFFu);
    }

    for (off = from; off < to; off += got)
    {
        run = hl_text(doc, off, &got);

        if (!run || !got)
        {
            break;
        }

        got = got > to - off ? to - off : got;

        for (i = 0u; i < got; i++)
        {
            c = run[i];

            if (high)
            {
                if (c >= 0xDC00u && c <= 0xDFFFu)
                {
                    emit(&s, 0x10000u + ((high - 0xD800u) << 10) + (c - 0xDC00u));
                    high = 0u;
                    continue;
                }

                emit(&s, 0xFFFDu);
                high = 0u;
            }

            if (c >= 0xD800u && c <= 0xDBFFu)
            {
                high = c;
            }

            else if (c == '\n' && eol != ED_EOL_LF)
            {
                emit(&s, '\r');

                if (eol == ED_EOL_CRLF)
                {
                    emit(&s, '\n');
                }
            }

            else
            {
                emit(&s, c >= 0xDC00u && c <= 0xDFFFu ? 0xFFFDu : c);
            }
        }
    }

    if (high)
    {
        emit(&s, 0xFFFDu);
    }

    *bytes = s.out;
    *n = s.n;

    return 0;
}

uint32_t ed_units_utf8(const uint16_t *text, uint32_t n, char *out, uint32_t max)
{
    uint32_t i = 0u;
    uint32_t k = 0u;
    uint32_t c;

    while (i < n && k + 4u <= max)
    {
        c = text[i++];

        if (c >= 0xD800u && c <= 0xDBFFu && i < n && text[i] >= 0xDC00u && text[i] <= 0xDFFFu)
        {
            c = 0x10000u + ((c - 0xD800u) << 10) + (text[i++] - 0xDC00u);
        }

        if (c < 0x80u)
        {
            out[k++] = (char)c;
        }

        else if (c < 0x800u)
        {
            out[k++] = (char)(0xC0u | c >> 6);
            out[k++] = (char)(0x80u | (c & 0x3Fu));
        }

        else if (c < 0x10000u)
        {
            out[k++] = (char)(0xE0u | c >> 12);
            out[k++] = (char)(0x80u | (c >> 6 & 0x3Fu));
            out[k++] = (char)(0x80u | (c & 0x3Fu));
        }

        else
        {
            out[k++] = (char)(0xF0u | c >> 18);
            out[k++] = (char)(0x80u | (c >> 12 & 0x3Fu));
            out[k++] = (char)(0x80u | (c >> 6 & 0x3Fu));
            out[k++] = (char)(0x80u | (c & 0x3Fu));
        }
    }

    return k;
}
