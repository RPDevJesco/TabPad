#include "hl.h"
#include "ed.h"
#include "port_hosted.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks;
static int fails;

#define CHECK(e) \
    do \
    { \
        checks++; \
        if (!(e)) \
        { \
            fails++; \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #e); \
        } \
    } while (0)

#define BARE 0xFFu
#define TEXT_MAX 8192u
#define PROBES 14

struct probe {
    const char *at;
    const char *row;
};

struct sample {
    const char *ext;
    struct probe probes[PROBES];
};

static const struct sample samples[] = {
    { ".c", { { "/* a sum", "comment" }, { "static", "keyword" }, { "uint32_t sum", "type" }, { "sum(", "function" }, { "0u", "number" }, { "total +=", 0 } } },
    { ".cpp", { { "// a class", "comment" }, { "template <", "keyword" }, { "Stack {", "type" }, { "push(const", "function" }, { "\"done\"", "string" }, { "42", "number" }, { "namespace", "keyword" } } },
    { ".cs", { { "// a small", "comment" }, { "namespace", "keyword" }, { "Greeter\n", "type" }, { "Greet(", "function" }, { "\"Hello", "string" }, { "this", "keyword" }, { "times)", "variable.parameter" } } },
    { ".css", { { "/* layout", "comment" }, { "@media", "keyword" }, { "color", "property" }, { "600", "number" }, { "\"bg.png\"", "string" }, { "a:hover", "tag" }, { "url(", "function" } } },
    { ".js", { { "// fetch", "comment" }, { "import", "keyword" }, { "\"utf8\"", "string" }, { "count(path", "function" }, { "LIMIT = 10", "constant" }, { "split(", "function" }, { "words.length}", "embedded" }, { "console", "variable.builtin" } } },
    { ".html", { { "<!-- the page", "comment" }, { "html lang", "tag" }, { "lang=", "attribute" }, { "en\"", "string" }, { "body class", "tag" }, { "Sample", 0 }, { "color: red", "property" }, { "const total", "keyword" }, { "42;", "number" }, { "// tally", "comment" } } },
    { ".lua", { { "-- a counter", "comment" }, { "local", "keyword" }, { "\"big", "string" }, { "if self", "conditional" }, { "for i", "repeat" }, { "bump(by", "method" }, { "{}", "none" }, { "10 then", "number" } } },
    { ".cls", { { "// an Apex", "comment" }, { "public", "keyword" }, { "AccountService", "class" }, { "'found ", "string" }, { "SELECT", "keyword" }, { "recent(", "method" }, { "AuraEnabled", "decorator" }, { "50;", "number" } } },
    { ".py", { { "# word", "comment" }, { "class", "keyword" }, { "__init__", "function" }, { "\"__main__\"", "string" }, { "None", "constant.builtin" }, { "most_common", "function" }, { "word}", "embedded" }, { "10)", "number" } } },
    { ".rb", { { "# a queue", "comment" }, { "class", "keyword" }, { "\"set\"", "string" }, { "LIMIT = 10", "constant" }, { "@name =", "property" }, { ":ok", "string.special.symbol" }, { "job} on", "embedded" }, { "initialize", "function" } } },
    { ".rs", { { "// a tally", "comment" }, { "struct", "keyword" }, { "Tally {\n    counts", "type" }, { "add(", "function" }, { "\"one\"", "string" }, { "u32>", "type" }, { "#[derive", "attribute" }, { "self.counts", "variable.builtin" } } },
    { ".md", { { "# Title", "punctuation.special" }, { "Title", "text.title" }, { "- first", "punctuation.special" }, { "```c", "text.literal" }, { "> quoted", "punctuation.special" }, { "Some text", 0 }, { "code` and", "text.literal" }, { "https://example", "text.uri" }, { "emphasis*", "text.emphasis" }, { "strong**", "text.strong" }, { "int x", "type" }, { "1;", "number" }, { "div class", "tag" }, { "print(", "none" } } },
    { ".json", { { "\"name\"", "string.special.key" }, { "\"sample\"", "string" }, { "3,", "number" }, { "true", "constant.builtin" }, { "null", "constant.builtin" }, { "1.5e3", "number" } } },
    { ".xml", { { "<!-- metadata", "comment" }, { "CustomObject xmlns", "tag" }, { "kind=", "property" }, { "\"text\"", "string" }, { "label>", "tag" }, { "Invoice", 0 } } },
    { ".sh", { { "# backs", "comment" }, { "if [", "keyword" }, { "backup()", "function" }, { "/backup\"", "string" }, { "for d", "keyword" }, { "HOME", "property" }, { "tar czf", "function" } } },
    { ".ps1", { { "# lists", "comment" }, { "function Get", "keyword" }, { "Get-Big {", "function" }, { "\".\"", "string" }, { "$Top = 5", "property" }, { "5)", "number" }, { "-gt", 0 }, { "string]", "type" } } },
};

#define SAMPLES (sizeof samples / sizeof samples[0])

static const char *folder;
static char text[TEXT_MAX];
static uint16_t wide[TEXT_MAX];
static uint32_t len;

static int read_sample(const char *ext)
{
    char path[512];
    FILE *f;
    uint32_t i;

    snprintf(path, sizeof path, "%s/sample%s", folder, ext);
    f = fopen(path, "rb");

    if (!f)
    {
        return -1;
    }

    len = (uint32_t)fread(text, 1u, TEXT_MAX - 1u, f);
    text[len] = 0;
    fclose(f);

    for (i = 0u; i < len; i++)
    {
        wide[i] = (uint8_t)text[i];
    }

    return 0;
}

static uint32_t row_of(const char *ext)
{
    uint32_t i;

    for (i = 0u; i < ed_ext_count && strcmp(ed_exts[i].ext, ext); i++)
    {
    }

    return i;
}

static void paint(int doc, uint8_t *ids)
{
    static uint32_t spans[256u * HL_SPAN_WORDS];
    uint32_t n = hl_len((uint32_t)doc);
    uint32_t from = 0u;
    uint32_t got;
    uint32_t at;
    uint32_t count;
    uint32_t i;
    uint32_t k;

    memset(ids, BARE, n);

    do
    {
        got = hl_spans((uint32_t)doc, from, n, spans, 256u);

        for (i = 0u; i < got; i++)
        {
            at = spans[i * HL_SPAN_WORDS + HL_SPAN_AT];
            count = spans[i * HL_SPAN_WORDS + HL_SPAN_LEN];
            CHECK(at >= from && count > 0u && at + count <= n);
            CHECK(spans[i * HL_SPAN_WORDS + HL_SPAN_ID] < hl_theme_count);

            for (k = 0u; k < count && at + k < n; k++)
            {
                ids[at + k] = (uint8_t)spans[i * HL_SPAN_WORDS + HL_SPAN_ID];
            }

            from = at + count;
        }
    } while (got == 256u);
}

static int probe_holds(const uint8_t *ids, const struct probe *p)
{
    const char *hit = strstr(text, p->at);
    uint8_t id;

    if (!hit)
    {
        return 0;
    }

    id = ids[hit - text];

    if (!p->row)
    {
        return id == BARE;
    }

    return id != BARE && !strcmp(hl_theme[id], p->row);
}

static uint32_t seed = 12345u;

static uint32_t rnd(uint32_t below)
{
    seed = seed * 1664525u + 1013904223u;

    return (seed >> 8) % below;
}

static void t_table(void)
{
    uint32_t i;
    uint32_t j;
    uint32_t seen = 0u;

    puts("the table: every ending names a language that is there, and a language has one name");
    CHECK(hl_lang_count == SAMPLES + 1u);
    CHECK(ed_exts[ed_ext_count - 1u].ext && !ed_exts[ed_ext_count].ext);

    for (i = 0u; i < ed_ext_count; i++)
    {
        CHECK(ed_exts[i].lang < hl_lang_count || (ed_exts[i].lang == HL_PLAIN && ed_exts[i].columns));
        CHECK(ed_exts[i].ext[0] == '.' && strlen(ed_exts[i].tag) < 16u && ed_exts[i].title[0] && ed_exts[i].name[0]);
        seen |= ed_exts[i].lang == HL_PLAIN ? 0u : 1u << ed_exts[i].lang;

        for (j = 0u; j < i; j++)
        {
            CHECK(strcmp(ed_exts[i].ext, ed_exts[j].ext));
            CHECK(!strcmp(ed_exts[i].tag, ed_exts[j].tag) == (ed_exts[i].lang == ed_exts[j].lang && ed_exts[i].columns == ed_exts[j].columns));
            CHECK(strcmp(ed_exts[i].tag, ed_exts[j].tag) || (!strcmp(ed_exts[i].title, ed_exts[j].title) && !strcmp(ed_exts[i].name, ed_exts[j].name)));
        }
    }

    CHECK(seen == (1u << SAMPLES) - 1u);
}

static void t_paint(void)
{
    static uint8_t ids[TEXT_MAX];
    uint32_t i;
    uint32_t k;
    uint32_t row;
    int doc;

    puts("every language opens its sample and paints it the way a reader expects");

    for (i = 0u; i < SAMPLES; i++)
    {
        row = row_of(samples[i].ext);
        CHECK(row < ed_ext_count && !read_sample(samples[i].ext));

        if (row == ed_ext_count)
        {
            continue;
        }

        doc = hl_open(ed_exts[row].lang, wide, len);
        CHECK(doc >= 0);
        paint(doc, ids);

        for (k = 0u; k < PROBES && samples[i].probes[k].at; k++)
        {
            if (!probe_holds(ids, &samples[i].probes[k]))
            {
                printf("     %s: \"%s\" is not drawn as %s\n", ed_exts[row].title, samples[i].probes[k].at, samples[i].probes[k].row ? samples[i].probes[k].row : "plain text");
            }

            CHECK(probe_holds(ids, &samples[i].probes[k]));
        }

        hl_close((uint32_t)doc);
    }
}

static void t_edits(void)
{
    static uint8_t first[TEXT_MAX];
    static uint8_t now[TEXT_MAX];
    static uint16_t piece[16];
    uint32_t i;
    uint32_t k;
    uint32_t round;
    uint32_t at;
    uint32_t cut;
    uint32_t n;
    uint32_t from;
    uint32_t size;
    int doc;

    puts("a sample cut up and pasted into at random still paints, and put back whole paints as at first");

    for (i = 0u; i < SAMPLES; i++)
    {
        if (row_of(samples[i].ext) == ed_ext_count || read_sample(samples[i].ext))
        {
            continue;
        }

        doc = hl_open(ed_exts[row_of(samples[i].ext)].lang, wide, len);
        CHECK(doc >= 0);
        paint(doc, first);

        for (round = 0u; round < 250u; round++)
        {
            size = hl_len((uint32_t)doc);
            at = rnd(size + 1u);
            cut = rnd(2u) ? rnd(12u) : 0u;
            cut = cut > size - at ? size - at : cut;
            n = size + 16u < TEXT_MAX && rnd(3u) ? rnd(12u) : 0u;
            from = rnd(len - n);

            for (k = 0u; k < n; k++)
            {
                piece[k] = wide[from + k];
            }

            CHECK(!hl_edit((uint32_t)doc, at, cut, piece, n));
            hl_settle();
            CHECK(hl_len((uint32_t)doc) == size - cut + n);

            if (round % 10u == 9u)
            {
                paint(doc, now);
            }
        }

        CHECK(!hl_edit((uint32_t)doc, 0u, hl_len((uint32_t)doc), wide, len));
        hl_settle();
        paint(doc, now);
        CHECK(hl_len((uint32_t)doc) == len && !memcmp(first, now, len));
        hl_close((uint32_t)doc);
    }
}

static void t_leaks(void)
{
    uint32_t live = 0u;
    uint32_t round;

    puts("the same edits made twice more: the second time keeps nothing the first did not");

    for (round = 0u; round < 2u; round++)
    {
        seed = 12345u;
        t_edits();
        CHECK(!round || hosted_port_live == live);
        live = hosted_port_live;
    }
}

int main(int argc, char **argv)
{
    folder = argc > 1 ? argv[1] : "test/samples";
    t_table();
    t_paint();
    t_edits();
    t_leaks();
    printf("%d checks, %d failed\n", checks, fails);

    return fails ? 1 : 0;
}
