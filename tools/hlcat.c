#include "hl_int.h"
#include "ed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const hl_theme[] = {
    "comment", "string", "number", "constant", "boolean", "keyword", "conditional", "repeat",
    "preproc", "type", "class", "interface", "enum", "module", "constructor", "function",
    "method", "decorator", "property", "field", "parameter", "attribute", "variable.parameter", "variable.builtin",
    "escape", "string.escape", "string.special", "tag", "punctuation.special", "text.title", "text.literal", "text.uri",
    "text.reference", "markup", "operator", "label", "embedded", "none",
};

const uint32_t hl_theme_count = sizeof hl_theme / sizeof hl_theme[0];

static const char *const colour[] = {
    "\x1b[90m", "\x1b[32m", "\x1b[33m", "\x1b[33;1m", "\x1b[33;1m", "\x1b[35m", "\x1b[35m", "\x1b[35m",
    "\x1b[35m", "\x1b[36m", "\x1b[36m", "\x1b[36m", "\x1b[36m", "\x1b[36m", "\x1b[36m", "\x1b[34;1m",
    "\x1b[34;1m", "\x1b[34;1m", "\x1b[94m", "\x1b[94m", "\x1b[94m", "\x1b[94m", "\x1b[94m", "\x1b[33;1m",
    "\x1b[33m", "\x1b[33m", "\x1b[31m", "\x1b[34m", "\x1b[34m", "\x1b[34;1m", "\x1b[32m", "\x1b[34;4m",
    "\x1b[94m", "\x1b[34;1m", "\x1b[37;1m", "\x1b[31m", "\x1b[39m", "\x1b[39m",
};

#define BATCH 256u

static char *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    char *p = 0;
    long size;

    if (!f)
    {
        return 0;
    }

    if (!fseek(f, 0, SEEK_END) && (size = ftell(f)) >= 0 && !fseek(f, 0, SEEK_SET))
    {
        p = malloc((size_t)size + 1u);
    }

    if (p)
    {
        *n = fread(p, 1u, (size_t)size, f);
    }

    fclose(f);

    return p;
}

static uint32_t lang_of(const char *path)
{
    size_t n = strlen(path);
    size_t k;
    uint32_t i;

    for (i = 0u; i < ed_ext_count; i++)
    {
        k = strlen(ed_exts[i].ext);

        if (k <= n && !strcmp(path + n - k, ed_exts[i].ext))
        {
            return ed_exts[i].lang;
        }
    }

    return HL_PLAIN;
}

int main(int argc, char **argv)
{
    uint32_t spans[BATCH * HL_SPAN_WORDS];
    const char *path = argc > 1 ? argv[argc - 1] : 0;
    const char *s;
    const char *e;
    char *src;
    uint16_t *text;
    size_t *byte_of;
    size_t size = 0u;
    uint32_t len = 0u;
    uint32_t from = 0u;
    uint32_t got;
    uint32_t at;
    uint32_t id;
    uint32_t i;
    uint32_t c;
    int listing = argc == 3 && !strcmp(argv[1], "--spans");
    int doc;

    src = argc == 2 || listing ? slurp(path, &size) : 0;

    if (!src)
    {
        fputs("usage: hlcat [--spans] <file>\n", stderr);

        return 2;
    }

    text = malloc((size + 1u) * sizeof *text);
    byte_of = malloc((size + 1u) * sizeof *byte_of);

    if (!text || !byte_of)
    {
        return 1;
    }

    s = src;
    e = src + size;

    while (s < e)
    {
        byte_of[len] = (size_t)(s - src);
        c = hl_utf8(&s, e);

        if (c >= 0x10000u)
        {
            byte_of[len + 1u] = byte_of[len];
            text[len++] = (uint16_t)(0xD800u + ((c - 0x10000u) >> 10));
            c = 0xDC00u + ((c - 0x10000u) & 0x3FFu);
        }

        text[len++] = (uint16_t)c;
    }

    byte_of[len] = size;
    doc = hl_open(lang_of(path), text, len);

    if (doc < 0)
    {
        fputs("hlcat: the file did not open\n", stderr);

        return 1;
    }

    do
    {
        got = hl_spans((uint32_t)doc, from, len, spans, BATCH);

        for (i = 0u; i < got; i++)
        {
            at = spans[i * HL_SPAN_WORDS + HL_SPAN_AT];
            id = spans[i * HL_SPAN_WORDS + HL_SPAN_ID];

            if (!listing)
            {
                fwrite(src + byte_of[from], 1u, byte_of[at] - byte_of[from], stdout);
            }

            from = at + spans[i * HL_SPAN_WORDS + HL_SPAN_LEN];
            fputs(listing ? hl_theme[id] : colour[id], stdout);
            fputs(listing ? ": " : "", stdout);
            fwrite(src + byte_of[at], 1u, byte_of[from] - byte_of[at], stdout);
            fputs(listing ? "\n" : "\x1b[0m", stdout);
        }
    } while (got == BATCH);

    if (!listing)
    {
        fwrite(src + byte_of[from], 1u, size - byte_of[from], stdout);
    }

    hl_close((uint32_t)doc);

    return 0;
}
