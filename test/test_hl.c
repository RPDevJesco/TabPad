#include "hl_int.h"
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

const struct TSLanguage *tree_sitter_c(void);

extern const char hl_query_c[];

enum {
    L_C,
    L_EQ,
    L_NOT_EQ,
    L_ANY_OF,
    L_NOT_ANY_OF,
    L_MATCH,
    L_NOT_MATCH,
    L_EQ_CAPTURE,
    L_IGNORED,
    L_LOCALS,
    L_NESTED,
    L_LATER,
    L_FALLBACK,
    L_UNICODE,
    L_BAD_PREDICATE,
    L_BAD_REGEX,
    L_BAD_SYNTAX,
    L_LUA,
    L_LUA_LAZY,
    L_CONTAINS,
    L_ANCESTOR,
    L_PARENT,
    L_NOT_ANCESTOR,
    L_BAD_LUA,
    L_NUMBERS,
    L_JOINED,
    L_LONE,
    L_LATE,
    L_COUNT
};

static const struct hl_inner in_joined[] = {
    { "string_content", "string_literal", L_NUMBERS, 1, 0 },
};

static const struct hl_inner in_lone[] = {
    { "string_content", 0, L_NUMBERS, 0, 0 },
};

struct hl_lang hl_langs[] = {
    { tree_sitter_c, hl_query_c, 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#eq? @keyword \"alpha\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#not-eq? @keyword \"alpha\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#any-of? @keyword \"alpha\" \"beta\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#not-any-of? @keyword \"alpha\" \"beta\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#match? @keyword \"^[a-c]+$\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#not-match? @keyword \"^[a-c]+$\"))", 0, 0u, 0 },
    { tree_sitter_c, "((assignment_expression left: (identifier) @keyword right: (identifier) @type) (#eq? @keyword @type))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#set! \"priority\" 105))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#is-not? local))", 0, 0u, 0 },
    { tree_sitter_c, "(string_literal) @string (escape_sequence) @constant", 0, 0u, 0 },
    { tree_sitter_c, "(identifier) @variable (identifier) @type", 0, 0u, 0 },
    { tree_sitter_c, "(identifier) @function.special.extra (number_literal) @nothing.known", 0, 0u, 0 },
    { tree_sitter_c, "((string_content) @keyword (#eq? @keyword \"h\xC3\xA9llo \xF0\x9F\x98\x80\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#frob? @keyword \"x\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#match? @keyword \"^\\\\p{Lu}\"))", 0, 0u, 0 },
    { tree_sitter_c, "(identifier", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#lua-match? @keyword \"^%l+a$\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#lua-match? @keyword \"^a.-[b-c]$\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#contains? @keyword \"et\" \"bc\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#has-ancestor? @keyword \"assignment_expression\" \"nothing\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#has-parent? @keyword \"compound_statement\" \"assignment_expression\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#not-has-ancestor? @keyword \"compound_statement\"))", 0, 0u, 0 },
    { tree_sitter_c, "((identifier) @keyword (#lua-match? @keyword \"%b()\"))", 0, 0u, 0 },
    { tree_sitter_c, "(number_literal) @number (primitive_type) @type", 0, 0u, 0 },
    { tree_sitter_c, "(comment) @comment (string_literal) @string", in_joined, 1u, 0 },
    { tree_sitter_c, "(comment) @comment (string_literal) @string", in_lone, 1u, 0 },
    { tree_sitter_c, "(number_literal) @number", 0, 0u, 0 },
};

uint32_t hl_lang_count = L_COUNT - 1u;

enum {
    T_COMMENT,
    T_STRING,
    T_NUMBER,
    T_CONSTANT,
    T_KEYWORD,
    T_TYPE,
    T_FUNCTION,
    T_FUNCTION_SPECIAL,
    T_VARIABLE,
    T_PROPERTY,
    T_OPERATOR,
    T_DELIMITER,
    T_LABEL,
    T_COUNT
};

const char *const hl_theme[] = {
    "comment", "string", "number", "constant", "keyword", "type", "function", "function.special",
    "variable", "property", "operator", "delimiter", "label",
};

const uint32_t hl_theme_count = T_COUNT;

#define BARE 0xFFu
#define TEXT_MAX 65536u
#define CHUNK 2048u

static uint16_t wide[TEXT_MAX];

static uint32_t w(const char *s)
{
    const char *e = s + strlen(s);
    uint32_t n = 0u;
    uint32_t c;

    while (s < e)
    {
        c = hl_utf8(&s, e);

        if (c >= 0x10000u)
        {
            wide[n++] = (uint16_t)(0xD800u + ((c - 0x10000u) >> 10));
            c = 0xDC00u + ((c - 0x10000u) & 0x3FFu);
        }

        wide[n++] = (uint16_t)c;
    }

    return n;
}

static int open_text(uint32_t lang, const char *s)
{
    uint32_t n = w(s);

    return hl_open(lang, wide, n);
}

static int edit_text(int doc, uint32_t at, uint32_t removed, const char *s)
{
    uint32_t n = w(s);

    int bad = hl_edit((uint32_t)doc, at, removed, wide, n);

    hl_settle();

    return bad;
}

static void paint(int doc, uint8_t *ids, uint32_t chunk)
{
    static uint32_t spans[CHUNK * HL_SPAN_WORDS];
    uint32_t len = hl_len((uint32_t)doc);
    uint32_t from = 0u;
    uint32_t got;
    uint32_t i;
    uint32_t k;
    uint32_t at;
    uint32_t n;

    memset(ids, BARE, len);

    do
    {
        got = hl_spans((uint32_t)doc, from, len, spans, chunk);

        for (i = 0u; i < got; i++)
        {
            at = spans[i * HL_SPAN_WORDS + HL_SPAN_AT];
            n = spans[i * HL_SPAN_WORDS + HL_SPAN_LEN];
            CHECK(at >= from && n > 0u && at + n <= len);
            CHECK(spans[i * HL_SPAN_WORDS + HL_SPAN_ID] < hl_theme_count);

            for (k = 0u; k < n; k++)
            {
                ids[at + k] = (uint8_t)spans[i * HL_SPAN_WORDS + HL_SPAN_ID];
            }

            from = at + n;
        }
    } while (got == chunk);
}

static uint8_t id_of(int doc, const char *text, const char *needle)
{
    static uint8_t ids[TEXT_MAX];
    const char *hit = strstr(text, needle);

    if (!hit)
    {
        return 0xFEu;
    }

    paint(doc, ids, CHUNK);

    return ids[hit - text];
}

static int re(const char *pat, const char *text)
{
    struct hl_buf b;
    uint32_t n = w(text);
    int hit;

    if (!hl_re_ok(pat, (uint32_t)strlen(pat)) || hl_buf_open(&b, wide, n))
    {
        return -1;
    }

    hit = hl_re_match(pat, (uint32_t)strlen(pat), &b, 0u, n);
    hl_buf_close(&b);

    return hit;
}

static void t_regex(void)
{
    puts("the regex: what it matches");
    CHECK(re("^[A-Z][A-Z\\d_]*$", "FOO_BAR2") == 1);
    CHECK(re("^[A-Z][A-Z\\d_]*$", "Foo") == 0);
    CHECK(re("^[A-Z][A-Z\\d_]*$", "") == 0);
    CHECK(re("^(true|false)$", "false") == 1);
    CHECK(re("^(true|false)$", "falsey") == 0);
    CHECK(re("^_*[A-Z]", "__Foo") == 1);
    CHECK(re("oo", "foo") == 1);
    CHECK(re("^oo", "foo") == 0);
    CHECK(re("fo$", "foo") == 0);
    CHECK(re("^a|b$", "xb") == 1);
    CHECK(re("^a|b$", "xa") == 0);
    CHECK(re("^(ab)+$", "ababab") == 1);
    CHECK(re("^(ab)+$", "ababa") == 0);
    CHECK(re("^(ab){2,3}$", "ab") == 0);
    CHECK(re("^(ab){2,3}$", "abab") == 1);
    CHECK(re("^(ab){2,3}$", "abababab") == 0);
    CHECK(re("^a{3}$", "aaa") == 1);
    CHECK(re("^a{2,}$", "a") == 0);
    CHECK(re("^(a|b)*c$", "ababc") == 1);
    CHECK(re("^(a*)*$", "aaa") == 1);
    CHECK(re("^(a?){3}$", "") == 1);
    CHECK(re("^(?:x|yz)+?$", "xyzx") == 1);
    CHECK(re("^.*_t$", "size_t") == 1);
    CHECK(re("^.*_t$", "size_tt") == 0);
    CHECK(re("^[^a-c]+$", "xyz") == 1);
    CHECK(re("^[^a-c]+$", "xbz") == 0);
    CHECK(re("^[\\w-]+$", "a-b_c") == 1);
    CHECK(re("^[a\\]]+$", "a]a") == 1);
    CHECK(re("^\\$\\w+$", "$var") == 1);
    CHECK(re("\\bfoo\\b", "a foo b") == 1);
    CHECK(re("\\bfoo\\b", "afoo b") == 0);
    CHECK(re("\\Boo", "foo") == 1);
    CHECK(re("^\\s*\\S+\\s*$", " \tx\n") == 1);
    CHECK(re("^\\d+\\D$", "42x") == 1);
    CHECK(re("^a.c$", "a\nc") == 0);
    CHECK(re("(?i)^select$", "SeLeCt") == 1);
    CHECK(re("(?i)^[a-c]+$", "ABC") == 1);
    CHECK(re("^select$", "SELECT") == 0);

    puts("the regex: characters are code points");
    CHECK(re("^.$", "\xF0\x9F\x98\x80") == 1);
    CHECK(re("^..$", "\xF0\x9F\x98\x80") == 0);
    CHECK(re("^h\xC3\xA9$", "h\xC3\xA9") == 1);
    CHECK(re("^[\xC3\xA0-\xC3\xBF]+$", "\xC3\xA9\xC3\xA8") == 1);
    CHECK(re("^\\w+$", "caf\xC3\xA9") == 1);
    CHECK(re("^.*\xF0\x9F\x98\x80x$", "a\xF0\x9F\x98\x80\xF0\x9F\x98\x80x") == 1);

    puts("the regex: what it refuses");
    CHECK(re("\\p{Lu}", "A") == -1);
    CHECK(re("(?=a)", "a") == -1);
    CHECK(re("(?m)^a", "a") == -1);
    CHECK(re("a(?i)b", "ab") == -1);
    CHECK(re("[[:alpha:]]", "a") == -1);
    CHECK(re("[a&&b]", "a") == -1);
    CHECK(re("[]", "a") == -1);
    CHECK(re("[a", "a") == -1);
    CHECK(re("(a", "a") == -1);
    CHECK(re("a)", "a") == -1);
    CHECK(re("*a", "a") == -1);
    CHECK(re("a**", "a") == -1);
    CHECK(re("a{", "a") == -1);
    CHECK(re("a{2,1}", "a") == -1);
    CHECK(re("a{99999}", "a") == -1);
    CHECK(re("^*", "a") == -1);
    CHECK(re("\\1", "a") == -1);
    CHECK(re("a\\", "a") == -1);

    puts("the regex: a hostile pattern runs out of budget, not time");
    CHECK(re("^(a|aa)+$", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab") == 0);
}

static const char sample[] =
    "#include <stdio.h>\n"
    "#define LIMIT 10\n"
    "/* a comment */\n"
    "struct point { int x; };\n"
    "static int add(int a, int b)\n"
    "{\n"
    "    struct point p;\n"
    "    p.x = a + LIMIT;\n"
    "    printf(\"sum\\n\");\n"
    "    return b + 42;\n"
    "}\n";

static void t_open(void)
{
    int doc;

    puts("a C file opens and every kind of token is painted");
    doc = open_text(L_C, sample);
    CHECK(doc >= 0);
    CHECK(hl_len((uint32_t)doc) == sizeof sample - 1u);
    CHECK(id_of(doc, sample, "#include") == T_KEYWORD);
    CHECK(id_of(doc, sample, "<stdio.h>") == T_STRING);
    CHECK(id_of(doc, sample, "/* a") == T_COMMENT);
    CHECK(id_of(doc, sample, "comment */") == T_COMMENT);
    CHECK(id_of(doc, sample, "struct") == T_KEYWORD);
    CHECK(id_of(doc, sample, "point {") == T_TYPE);
    CHECK(id_of(doc, sample, "int x") == T_TYPE);
    CHECK(id_of(doc, sample, "x; }") == T_PROPERTY);
    CHECK(id_of(doc, sample, "static") == T_KEYWORD);
    CHECK(id_of(doc, sample, "add(") == T_FUNCTION);
    CHECK(id_of(doc, sample, "a, int") == T_VARIABLE);
    CHECK(id_of(doc, sample, "= a") == T_OPERATOR);
    CHECK(id_of(doc, sample, "printf") == T_FUNCTION);
    CHECK(id_of(doc, sample, "\"sum") == T_STRING);
    CHECK(id_of(doc, sample, "return") == T_KEYWORD);
    CHECK(id_of(doc, sample, "42") == T_NUMBER);
    CHECK(id_of(doc, sample, "; }") == T_DELIMITER);
    CHECK(id_of(doc, sample, " int a") == BARE);

    puts("#match? separates a constant from a variable");
    CHECK(id_of(doc, sample, "LIMIT;") == T_CONSTANT);
    CHECK(id_of(doc, sample, "b + 42") == T_VARIABLE);
    hl_close((uint32_t)doc);
    CHECK(hl_len((uint32_t)doc) == 0u);
}

static void t_predicates(void)
{
    static const char text[] = "int alpha, beta, gamma, abc; void f(void) { alpha = alpha; beta = gamma; }";
    static const struct {
        uint32_t lang;
        uint8_t alpha, beta, gamma, abc;
    } want[] = {
        { L_EQ, T_KEYWORD, BARE, BARE, BARE },
        { L_NOT_EQ, BARE, T_KEYWORD, T_KEYWORD, T_KEYWORD },
        { L_ANY_OF, T_KEYWORD, T_KEYWORD, BARE, BARE },
        { L_NOT_ANY_OF, BARE, BARE, T_KEYWORD, T_KEYWORD },
        { L_MATCH, BARE, BARE, BARE, T_KEYWORD },
        { L_NOT_MATCH, T_KEYWORD, T_KEYWORD, T_KEYWORD, BARE },
        { L_IGNORED, T_KEYWORD, T_KEYWORD, T_KEYWORD, T_KEYWORD },
        { L_LOCALS, BARE, BARE, BARE, BARE },
    };
    uint32_t i;
    int doc;

    puts("each predicate, alone in its own query");

    for (i = 0u; i < sizeof want / sizeof want[0]; i++)
    {
        doc = open_text(want[i].lang, text);
        CHECK(doc >= 0);
        CHECK(id_of(doc, text, "alpha") == want[i].alpha);
        CHECK(id_of(doc, text, "beta") == want[i].beta);
        CHECK(id_of(doc, text, "gamma") == want[i].gamma);
        CHECK(id_of(doc, text, "abc") == want[i].abc);
        hl_close((uint32_t)doc);
    }

    puts("#eq? between two captures");
    doc = open_text(L_EQ_CAPTURE, text);
    CHECK(id_of(doc, text, "alpha = alpha") == T_KEYWORD);
    CHECK(id_of(doc, text, "alpha; beta") == T_TYPE);
    CHECK(id_of(doc, text, "beta = gamma") == BARE);
    CHECK(id_of(doc, text, "gamma; }") == BARE);
    hl_close((uint32_t)doc);
}

static void t_neovim(void)
{
    static const char text[] = "int alpha, beta, gamma, abc; void f(void) { alpha = alpha; beta = gamma; }";
    int doc;

    puts("the predicates Neovim's queries use: Lua patterns, contains, parents and ancestors");
    doc = open_text(L_LUA, text);
    CHECK(id_of(doc, text, "alpha") == T_KEYWORD && id_of(doc, text, "beta") == T_KEYWORD && id_of(doc, text, "abc") == BARE);
    hl_close((uint32_t)doc);
    doc = open_text(L_LUA_LAZY, text);
    CHECK(id_of(doc, text, "alpha") == BARE && id_of(doc, text, "beta") == BARE && id_of(doc, text, "abc") == T_KEYWORD);
    hl_close((uint32_t)doc);
    doc = open_text(L_CONTAINS, text);
    CHECK(id_of(doc, text, "alpha") == BARE && id_of(doc, text, "beta") == T_KEYWORD && id_of(doc, text, "abc") == T_KEYWORD);
    hl_close((uint32_t)doc);
    doc = open_text(L_ANCESTOR, text);
    CHECK(id_of(doc, text, "alpha") == BARE && id_of(doc, text, "alpha = alpha") == T_KEYWORD && id_of(doc, text, "gamma; }") == T_KEYWORD);
    hl_close((uint32_t)doc);
    doc = open_text(L_PARENT, text);
    CHECK(id_of(doc, text, "alpha") == BARE && id_of(doc, text, "alpha = alpha") == T_KEYWORD && id_of(doc, text, "f(void)") == BARE);
    hl_close((uint32_t)doc);
    doc = open_text(L_NOT_ANCESTOR, text);
    CHECK(id_of(doc, text, "alpha") == T_KEYWORD && id_of(doc, text, "alpha = alpha") == BARE && id_of(doc, text, "f(void)") == T_KEYWORD);
    hl_close((uint32_t)doc);
    CHECK(open_text(L_BAD_LUA, text) < 0);
}

static void t_inner(void)
{
    static const char text[] = "char *a = \"int x = 42;\"; /* 1 */ char *b = \"7 + 8\";";
    static const char more[] = "char *a = \"int x = 42;\"; /* 1 */ char *b = \"7 + 8\"; char *c = \"5 \\n 6\";";
    static const uint32_t langs[] = { L_JOINED, L_LONE };
    uint32_t live = 0u;
    uint32_t from;
    uint32_t to;
    uint32_t round;
    uint32_t i;
    int doc;

    puts("a language inside a language: the inside is painted by its own grammar, and follows edits");

    for (round = 0u; round < 5u; round++)
    {
        for (i = 0u; i < 2u; i++)
        {
            doc = open_text(langs[i], text);
            CHECK(doc >= 0);
            CHECK(id_of(doc, text, "42") == T_NUMBER && id_of(doc, text, "int x") == T_TYPE && id_of(doc, text, "7 +") == T_NUMBER);
            CHECK(id_of(doc, text, "/* 1") == T_COMMENT && id_of(doc, text, "char") == BARE && id_of(doc, text, "x = 42") == T_STRING);
            CHECK(!hl_dirty((uint32_t)doc, &from, &to));
            CHECK(!edit_text(doc, sizeof text - 1u, 0u, more + sizeof text - 1u));
            CHECK(hl_dirty((uint32_t)doc, &from, &to) && to == sizeof more - 1u);
            CHECK(id_of(doc, more, "5 ") == T_NUMBER && id_of(doc, more, "6\"") == T_NUMBER && id_of(doc, more, "42") == T_NUMBER);
            CHECK(!edit_text(doc, (uint32_t)(strstr(more, "42") - more), 2u, "y"));
            CHECK(hl_dirty((uint32_t)doc, &from, &to) && from <= (uint32_t)(strstr(more, "42") - more));
            CHECK(!edit_text(doc, 0u, hl_len((uint32_t)doc), text));
            CHECK(id_of(doc, text, "42") == T_NUMBER && id_of(doc, text, "7 +") == T_NUMBER && id_of(doc, text, "/* 1") == T_COMMENT);
            CHECK(!edit_text(doc, 0u, hl_len((uint32_t)doc), "int y = 3;"));
            CHECK(id_of(doc, "int y = 3;", "3") == BARE);
            hl_close((uint32_t)doc);
        }

        CHECK(round < 4u || hosted_port_live == live);
        live = hosted_port_live;
    }
}

static void t_late_parse(void)
{
    static char big[60008];
    static const char line[] = "int value = 12345;\n";
    uint32_t from = 0u;
    uint32_t to = 0u;
    uint32_t n = 0u;
    uint32_t len;
    int doc;

    puts("a change too big to parse between two keys waits for a pause, and the text is never behind");

    while (n + sizeof line < sizeof big)
    {
        memcpy(big + n, line, sizeof line - 1u);
        n += (uint32_t)sizeof line - 1u;
    }

    big[n] = 0;
    doc = open_text(L_C, "int x;");
    CHECK(doc >= 0 && !hl_settle());
    len = w(big);
    CHECK(!hl_edit((uint32_t)doc, 0u, 6u, wide, len) && hl_len((uint32_t)doc) == n && hl_rows((uint32_t)doc) == n / 19u + 1u);
    id_of(doc, big, "12345");
    CHECK(!hl_edit((uint32_t)doc, 4u, 5u, wide, 3u) && hl_len((uint32_t)doc) == n - 2u);
    id_of(doc, big, "12345");
    CHECK(hl_settle() == 1u && !hl_settle());
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from == 0u && to == n - 2u);
    CHECK(!hl_edit((uint32_t)doc, 4u, 3u, wide + 4u, 5u));
    CHECK(id_of(doc, big, "12345") == T_NUMBER && id_of(doc, big, "int") == T_TYPE && id_of(doc, big, "value") == T_VARIABLE);
    CHECK(!edit_text(doc, 4u, 0u, "x") && !hl_settle());
    hl_close((uint32_t)doc);
}

static int find(int doc, const char *regex, uint32_t from, uint32_t to, uint32_t *at, uint32_t *len)
{
    return hl_find((uint32_t)doc, regex, (uint32_t)strlen(regex), from, to, at, len, 0);
}

static void t_find(void)
{
    static const char text[] = "one two\nthree two\nTWO\n";
    uint32_t groups[HL_GROUPS * 2u];
    uint32_t at;
    uint32_t len;
    int doc;

    puts("a search by regular expression: the first match from a point, by line");
    doc = open_text(HL_PLAIN, text);
    CHECK(find(doc, "two", 0u, 99u, &at, &len) == 1 && at == 4u && len == 3u);
    CHECK(find(doc, "two", 5u, 99u, &at, &len) == 1 && at == 14u && len == 3u);
    CHECK(find(doc, "two", 15u, 99u, &at, &len) == 0);
    CHECK(find(doc, "(?i)two", 15u, 99u, &at, &len) == 1 && at == 18u);
    CHECK(find(doc, "two", 0u, 4u, &at, &len) == 0 && find(doc, "two", 0u, 5u, &at, &len) == 1);
    CHECK(find(doc, "^t\\w+", 0u, 99u, &at, &len) == 1 && at == 8u && len == 5u);
    CHECK(find(doc, "o$", 0u, 99u, &at, &len) == 1 && at == 6u && len == 1u);
    CHECK(find(doc, "t.o|thr", 5u, 99u, &at, &len) == 1 && at == 8u && len == 3u);
    CHECK(find(doc, "\\w+ \\w+", 0u, 99u, &at, &len) == 1 && at == 0u && len == 7u);
    CHECK(find(doc, "e.x", 0u, 99u, &at, &len) == 0);
    CHECK(find(doc, "a*", 0u, 99u, &at, &len) == 0);
    CHECK(find(doc, "x", 0u, 99u, &at, &len) == 0);
    CHECK(find(doc, "(", 0u, 99u, &at, &len) == -1);
    CHECK(find(99, "two", 0u, 99u, &at, &len) == -1);

    puts("a search can say where each group of the expression matched");
    CHECK(hl_find((uint32_t)doc, "(\\w+) (t(w)o)", 13u, 0u, 99u, &at, &len, groups) == 1 && at == 0u && len == 7u);
    CHECK(groups[0] == 0u && groups[1] == 7u && groups[2] == 0u && groups[3] == 3u && groups[4] == 4u && groups[5] == 7u && groups[6] == 5u && groups[7] == 6u && groups[8] == 0xFFFFFFFFu);
    CHECK(hl_find((uint32_t)doc, "(x)|(?:t)(wo)", 13u, 0u, 99u, &at, &len, groups) == 1 && at == 4u);
    CHECK(groups[2] == 0xFFFFFFFFu && groups[4] == 5u && groups[5] == 7u);
    CHECK(hl_find((uint32_t)doc, "(?:(e)|(.))+ ", 13u, 8u, 99u, &at, &len, groups) == 1 && at == 8u && len == 6u);
    CHECK(groups[2] == 12u && groups[3] == 13u && groups[4] == 10u && groups[5] == 11u);
    hl_close((uint32_t)doc);
}

static void t_refused(void)
{
    uint32_t live;

    puts("a query the core cannot evaluate exactly is refused, every time");
    CHECK(open_text(L_BAD_PREDICATE, "int x;") < 0);
    CHECK(open_text(L_BAD_REGEX, "int x;") < 0);
    CHECK(open_text(L_BAD_SYNTAX, "int x;") < 0);
    live = hosted_port_live;
    CHECK(open_text(L_BAD_PREDICATE, "int x;") < 0);
    CHECK(open_text(L_BAD_REGEX, "int x;") < 0);
    CHECK(open_text(L_BAD_SYNTAX, "int x;") < 0);
    CHECK(hosted_port_live == live);
}

static void t_flatten(void)
{
    static const char text[] = "char *s = \"ab\\ncd\"; int n = 7;";
    uint32_t spans[8 * HL_SPAN_WORDS];
    uint32_t quote = (uint32_t)(strchr(text, '"') - text);
    int doc;

    puts("a capture inside a capture is cut out of it");
    doc = open_text(L_NESTED, text);
    CHECK(hl_spans((uint32_t)doc, 0u, sizeof text, spans, 8u) == 3u);
    CHECK(spans[0] == quote && spans[1] == 3u && spans[2] == T_STRING);
    CHECK(spans[3] == quote + 3u && spans[4] == 2u && spans[5] == T_CONSTANT);
    CHECK(spans[6] == quote + 5u && spans[7] == 3u && spans[8] == T_STRING);

    puts("a range is clipped to, and a full buffer can be resumed");
    CHECK(hl_spans((uint32_t)doc, quote + 1u, quote + 4u, spans, 8u) == 2u);
    CHECK(spans[0] == quote + 1u && spans[1] == 2u && spans[2] == T_STRING);
    CHECK(spans[3] == quote + 3u && spans[4] == 1u && spans[5] == T_CONSTANT);
    CHECK(hl_spans((uint32_t)doc, 0u, sizeof text, spans, 2u) == 2u);
    CHECK(hl_spans((uint32_t)doc, spans[3] + spans[4], sizeof text, spans, 2u) == 1u);
    CHECK(spans[0] == quote + 5u && spans[2] == T_STRING);
    CHECK(hl_spans((uint32_t)doc, 0u, quote, spans, 8u) == 0u);
    CHECK(hl_spans((uint32_t)doc, 5u, 5u, spans, 8u) == 0u);
    CHECK(hl_spans((uint32_t)doc, 0u, sizeof text, spans, 0u) == 0u);
    hl_close((uint32_t)doc);

    puts("two patterns on one node: the later one wins");
    doc = open_text(L_LATER, text);
    CHECK(id_of(doc, text, "s =") == T_TYPE);
    hl_close((uint32_t)doc);

    puts("a capture falls back to its longest dotted prefix, or to nothing");
    doc = open_text(L_FALLBACK, text);
    CHECK(id_of(doc, text, "s =") == T_FUNCTION_SPECIAL);
    CHECK(id_of(doc, text, "7") == BARE);
    hl_close((uint32_t)doc);
}

static void t_utf16(void)
{
    static const char text[] = "char *s = \"h\xC3\xA9llo \xF0\x9F\x98\x80\"; int after;";
    static const uint16_t low = 0xDE00u;
    uint32_t spans[4 * HL_SPAN_WORDS];
    uint32_t row;
    uint32_t col;
    int doc;

    puts("offsets are UTF-16 units: a surrogate pair is two");
    doc = open_text(L_UNICODE, text);
    CHECK(hl_len((uint32_t)doc) == sizeof text - 1u - 1u - 2u);
    CHECK(hl_spans((uint32_t)doc, 0u, 100u, spans, 4u) == 1u);
    CHECK(spans[0] == 11u && spans[1] == 8u && spans[2] == T_KEYWORD);
    CHECK(!hl_row_col((uint32_t)doc, 21u, &row, &col) && row == 0u && col == 21u);
    hl_close((uint32_t)doc);

    doc = open_text(L_C, text);
    CHECK(hl_spans((uint32_t)doc, 20u, 23u, spans, 4u) == 2u);
    CHECK(spans[0] == 20u && spans[1] == 1u && spans[2] == T_DELIMITER);
    CHECK(spans[3] == 22u && spans[4] == 1u && spans[5] == T_TYPE);

    puts("an edit may split a pair and put it back");
    CHECK(!edit_text(doc, 18u, 1u, ""));
    CHECK(hl_len((uint32_t)doc) == sizeof text - 5u);
    CHECK(!hl_edit((uint32_t)doc, 18u, 0u, &low, 1u));
    CHECK(hl_spans((uint32_t)doc, 20u, 23u, spans, 4u) == 2u);
    CHECK(spans[0] == 20u && spans[2] == T_DELIMITER);
    CHECK(spans[3] == 22u && spans[5] == T_TYPE);
    hl_close((uint32_t)doc);
}

static void t_edit(void)
{
    static const char text[] = "int foo;\nint bar;\nint baz; /* end */\n";
    static const char commented[] = "/*int Fo;\nint bar;\nint baz; /* end */\n";
    uint32_t from;
    uint32_t to;
    int doc;

    puts("typing changes what a token is");
    doc = open_text(L_C, text);
    CHECK(!hl_dirty((uint32_t)doc, &from, &to));
    CHECK(id_of(doc, text, "foo") == T_VARIABLE);
    CHECK(!edit_text(doc, 7u, 0u, "(void)"));
    CHECK(id_of(doc, "int foo(void);", "foo") == T_FUNCTION);
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from <= 4u && to >= 13u);
    CHECK(!hl_dirty((uint32_t)doc, &from, &to));

    puts("a rename that keeps the tree's shape still reports its token");
    CHECK(!edit_text(doc, 4u, 3u, "FOO"));
    CHECK(!edit_text(doc, 7u, 6u, ""));
    CHECK(id_of(doc, "int FOO;", "FOO") == T_CONSTANT);
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from <= 4u && to >= 7u);
    CHECK(!edit_text(doc, 6u, 1u, ""));
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from <= 4u && to >= 6u);
    CHECK(!edit_text(doc, 5u, 1u, "o"));
    CHECK(id_of(doc, "int Fo;", "Fo") == T_VARIABLE);
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from <= 4u && to >= 6u);

    puts("opening a comment repaints as far as it now reaches, removing it repaints back");
    CHECK(!edit_text(doc, 0u, 0u, "/*"));
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from == 0u && to >= (uint32_t)(strstr(commented, "*/") - commented) + 2u);
    CHECK(id_of(doc, commented, "Fo") == T_COMMENT);
    CHECK(id_of(doc, commented, "bar") == T_COMMENT);
    CHECK(id_of(doc, commented, "end") == T_COMMENT);
    CHECK(!edit_text(doc, 0u, 2u, ""));
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from == 0u && to >= (uint32_t)(strstr(commented, "*/") - commented));
    CHECK(id_of(doc, commented + 2, "Fo") == T_VARIABLE);
    CHECK(id_of(doc, commented + 2, "bar") == T_VARIABLE);
    CHECK(id_of(doc, commented + 2, "end") == T_COMMENT);

    puts("an edit outside the text is refused and changes nothing");
    CHECK(edit_text(doc, hl_len((uint32_t)doc) + 1u, 0u, "x") < 0);
    CHECK(edit_text(doc, 0u, hl_len((uint32_t)doc) + 1u, "") < 0);
    CHECK(hl_edit((uint32_t)doc, 0u, 0u, 0, 1u) < 0);
    CHECK(hl_edit(999u, 0u, 0u, wide, 1u) < 0);
    CHECK(!hl_dirty((uint32_t)doc, &from, &to));
    CHECK(id_of(doc, commented + 2, "bar") == T_VARIABLE);

    puts("everything can be deleted, and typed again");
    CHECK(!edit_text(doc, 0u, hl_len((uint32_t)doc), ""));
    CHECK(hl_len((uint32_t)doc) == 0u);
    CHECK(!edit_text(doc, 0u, 0u, "return 1;"));
    CHECK(id_of(doc, "return 1;", "return") == T_KEYWORD);
    hl_close((uint32_t)doc);
}

static void t_plain(void)
{
    uint32_t spans[HL_SPAN_WORDS];
    uint32_t off;
    uint32_t from;
    uint32_t to;
    int doc;

    puts("a document in no language: text, rows and edits, never a span");
    doc = open_text(HL_PLAIN, "int a;\nint b;\n");
    CHECK(doc >= 0 && hl_len((uint32_t)doc) == 14u && hl_rows((uint32_t)doc) == 3u);
    CHECK(hl_spans((uint32_t)doc, 0u, 14u, spans, 1u) == 0u);
    CHECK(!hl_row_start((uint32_t)doc, 1u, &off) && off == 7u);
    CHECK(!hl_row_start((uint32_t)doc, 2u, &off) && off == 14u);
    CHECK(hl_row_start((uint32_t)doc, 3u, &off) < 0);
    CHECK(!edit_text(doc, 7u, 0u, "x\ny\n"));
    CHECK(hl_rows((uint32_t)doc) == 5u && !hl_row_start((uint32_t)doc, 3u, &off) && off == 11u);
    CHECK(hl_dirty((uint32_t)doc, &from, &to) && from == 7u && to == 11u);
    CHECK(!edit_text(doc, 0u, 9u, ""));
    CHECK(hl_rows((uint32_t)doc) == 3u && !hl_row_start((uint32_t)doc, 0u, &off) && off == 0u);
    hl_close((uint32_t)doc);
}

static void t_bad_arguments(void)
{
    uint32_t spans[HL_SPAN_WORDS];
    uint32_t n;
    uint32_t row;
    uint32_t col;

    puts("calls on nothing are no-ops");
    CHECK(hl_open(L_COUNT, wide, 0u) < 0);
    CHECK(hl_open(L_C, 0, 1u) < 0);
    CHECK(hl_spans(999u, 0u, 10u, spans, 1u) == 0u);
    CHECK(hl_len(999u) == 0u);
    CHECK(hl_text(999u, 0u, &n) == 0 && n == 0u);
    CHECK(hl_row_col(999u, 0u, &row, &col) < 0);
    CHECK(!hl_dirty(999u, &row, &col));
    hl_close(999u);
}

static uint16_t model[TEXT_MAX];
static uint32_t model_len;
static uint32_t seed = 0x2545F491u;

static uint32_t rnd(uint32_t below)
{
    seed = seed * 1664525u + 1013904223u;

    return (seed >> 8) % below;
}

static void model_edit(uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    memmove(model + at + n, model + at + removed, (model_len - at - removed) * sizeof model[0]);
    memcpy(model + at, ins, n * sizeof model[0]);
    model_len = model_len - removed + n;
}

static int text_matches(int doc)
{
    const uint16_t *run;
    uint32_t off = 0u;
    uint32_t n;

    if (hl_len((uint32_t)doc) != model_len)
    {
        return 0;
    }

    for (;;)
    {
        run = hl_text((uint32_t)doc, off, &n);

        if (!n)
        {
            return off == model_len;
        }

        if (off + n > model_len || memcmp(run, model + off, n * sizeof model[0]))
        {
            return 0;
        }

        off += n;
    }
}

static int row_col_matches(int doc, uint32_t off)
{
    uint32_t row = 0u;
    uint32_t col = 0u;
    uint32_t got_row;
    uint32_t got_col;
    uint32_t i;

    for (i = 0u; i < off; i++)
    {
        col++;

        if (model[i] == '\n')
        {
            row++;
            col = 0u;
        }
    }

    if (hl_row_col((uint32_t)doc, off, &got_row, &got_col) || got_row != row || got_col != col)
    {
        return 0;
    }

    if (hl_row_start((uint32_t)doc, row, &got_col) || got_col != off - col)
    {
        return 0;
    }

    for (i = off; i < model_len; i++)
    {
        row += model[i] == '\n';
    }

    return hl_rows((uint32_t)doc) == row + 1u && hl_row_start((uint32_t)doc, row + 1u, &got_col) < 0;
}

struct tally {
    int text, rows, paint, dirty;
};

static void fuzz_edit(int doc, struct tally *bad, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    static uint8_t was[TEXT_MAX];
    static uint8_t now[TEXT_MAX];
    static uint8_t ref[TEXT_MAX];
    uint32_t from;
    uint32_t to;
    uint32_t i;
    int dirty;
    int fresh;

    paint(doc, was, CHUNK);
    CHECK(!hl_edit((uint32_t)doc, at, removed, ins, n));
    hl_settle();
    model_edit(at, removed, ins, n);
    bad->text += !text_matches(doc);
    bad->rows += !row_col_matches(doc, rnd(model_len + 1u));
    dirty = hl_dirty((uint32_t)doc, &from, &to);
    paint(doc, now, rnd(16u) ? CHUNK : 7u);
    fresh = hl_open(L_C, model, model_len);
    paint(fresh, ref, CHUNK);
    hl_close((uint32_t)fresh);
    bad->paint += memcmp(now, ref, model_len) != 0;

    for (i = 0u; i < model_len; i++)
    {
        if (dirty && i >= from && i < to)
        {
            continue;
        }

        if ((i >= at && i < at + n) || now[i] != was[i < at ? i : i - n + removed])
        {
            bad->dirty++;
            break;
        }
    }
}

static void t_fuzz_valid(void)
{
    static const char *const items[] = {
        "int x = 1;\n",
        "#define LIMIT 10\n",
        "#include <stdio.h>\n",
        "/* a note\n   on two lines */\n",
        "// a line\n",
        "\n",
        "struct point { int x; int y; };\n",
        "enum colour { RED, GREEN };\n",
        "typedef unsigned long size_type;\n",
        "const char *s = \"a\\tb \xC3\xA9 \xF0\x9F\x98\x80\";\n",
        "static int add(int a, int b)\n{\n    return a + b + LIMIT;\n}\n",
        "void f(void)\n{\n    struct point p;\n    p.x = MAX;\n    if (p.x) { goto out; }\nout:\n    printf(\"%d\\n\", p.y);\n}\n",
    };

    static uint32_t starts[102];
    struct tally bad = { 0, 0, 0, 0 };
    uint32_t count = 0u;
    uint32_t round;
    uint32_t k;
    uint32_t n;
    uint32_t i;
    int doc;

    puts("1000 edits that keep the text valid C: an edited document equals a fresh one, exactly");
    model_len = 0u;
    doc = hl_open(L_C, model, 0u);
    CHECK(doc >= 0);

    for (round = 0u; round < 1000u; round++)
    {
        k = rnd(count + 1u);

        if (count == 100u || (count && !rnd(3u)))
        {
            k %= count;
            n = starts[k + 1u] - starts[k];
            fuzz_edit(doc, &bad, starts[k], n, model, 0u);

            for (i = k + 1u; i < count; i++)
            {
                starts[i] = starts[i + 1u] - n;
            }

            count--;
            continue;
        }

        n = w(items[rnd(sizeof items / sizeof items[0])]);
        fuzz_edit(doc, &bad, starts[k], 0u, wide, n);

        for (i = count + 1u; i > k; i--)
        {
            starts[i] = starts[i - 1u] + n;
        }

        count++;
    }

    CHECK(bad.text == 0);
    CHECK(bad.rows == 0);
    CHECK(bad.paint == 0);
    CHECK(bad.dirty == 0);
    hl_close((uint32_t)doc);
}

static void t_fuzz_broken(void)
{
    static const char *const bits[] = {
        "/*", "*/", "//", "\"", "'", "\n", "\n\n", " ", "(", ")", "{", "}", ";", ",", "=", "+", "#define X 1\n",
        "#include <a.h>\n", "int ", "struct ", "return ", "if (", "else ", "FOO", "foo", "x", "_t", "42", "0x", ".",
        "->", "\\n", "\\", "\xC3\xA9", "\xF0\x9F\x98\x80", "static void f(void)\n{\n}\n", "case 1:", "label:", "\t",
    };
    struct tally bad = { 0, 0, 0, 0 };
    uint16_t ins[64];
    uint32_t round;
    uint32_t at;
    uint32_t removed;
    uint32_t n;
    int doc;

    puts("2000 edits of garbage: the text, the rows and hl_dirty stay exact");
    model_len = w(sample);
    memcpy(model, wide, model_len * sizeof model[0]);
    doc = hl_open(L_C, model, model_len);
    CHECK(doc >= 0);

    for (round = 0u; round < 2000u; round++)
    {
        at = rnd(model_len + 1u);
        removed = rnd(4u) ? 0u : rnd(12u);
        removed = removed > model_len - at ? model_len - at : removed;
        n = rnd(5u) ? w(bits[rnd(sizeof bits / sizeof bits[0])]) : 0u;
        memcpy(ins, wide, n * sizeof ins[0]);

        if (model_len > 3000u)
        {
            removed = model_len - at > 200u ? 200u : model_len - at;
        }

        fuzz_edit(doc, &bad, at, removed, ins, n);
    }

    CHECK(bad.text == 0);
    CHECK(bad.rows == 0);
    CHECK(bad.dirty == 0);
    printf("and the paint differed from a fresh document's in %d rounds: almost never\n", bad.paint);
    CHECK(bad.paint <= 3);
    hl_close((uint32_t)doc);
}

static void t_many(void)
{
    int docs[40];
    uint32_t i;

    puts("forty documents at once stay independent");

    for (i = 0u; i < 40u; i++)
    {
        docs[i] = open_text(L_C, i % 2u ? "int odd;" : "return;");
        CHECK(docs[i] >= 0);
    }

    CHECK(!edit_text(docs[3], 4u, 3u, "ODD"));
    CHECK(id_of(docs[3], "int ODD;", "ODD") == T_CONSTANT);
    CHECK(id_of(docs[5], "int odd;", "odd") == T_VARIABLE);
    CHECK(id_of(docs[38], "return;", "return") == T_KEYWORD);

    for (i = 0u; i < 40u; i++)
    {
        hl_close((uint32_t)docs[i]);
    }

    puts("a closed slot is handed out again");
    CHECK(open_text(L_C, "int x;") == docs[0]);
    hl_close((uint32_t)docs[0]);
}

static void t_no_memory(void)
{
    static const char text[] = "int x;";
    static char big[4000];
    uint32_t live;
    int doc;

    puts("no memory at open: nothing is opened, nothing is kept");
    live = hosted_port_live;
    hosted_port_fail_alloc(1u);
    CHECK(open_text(L_C, text) < 0);
    CHECK(hosted_port_live == live);

    puts("no memory to grow the text: the edit is refused and the document is as it was");
    doc = open_text(L_C, text);
    memset(big, ' ', sizeof big - 1u);
    hosted_port_fail_alloc(1u);
    CHECK(edit_text(doc, 3u, 0u, big) < 0);
    hosted_port_fail_alloc(0u);
    CHECK(hl_len((uint32_t)doc) == sizeof text - 1u);
    CHECK(id_of(doc, text, "x") == T_VARIABLE);
    CHECK(!edit_text(doc, 3u, 0u, big));
    CHECK(hl_len((uint32_t)doc) == sizeof text - 1u + sizeof big - 1u);
    hl_close((uint32_t)doc);
}

static void t_late(void)
{
    int doc;

    puts("a language the port counts in after others have loaded opens like any other");
    CHECK(!hl_lang_ok(L_LATE) && open_text(L_LATE, "int x = 7;") < 0);
    hl_lang_count = L_COUNT;
    CHECK(hl_lang_ok(L_LATE) && hl_lang_ok(L_C) && !hl_lang_ok(L_BAD_REGEX) && !hl_lang_ok(L_COUNT));
    doc = open_text(L_LATE, "int x = 7;");
    CHECK(doc >= 0 && id_of(doc, "int x = 7;", "7") == T_NUMBER && id_of(doc, "int x = 7;", "int") == BARE);
    hl_close((uint32_t)doc);
    doc = open_text(L_C, "int x = 7;");
    CHECK(doc >= 0 && id_of(doc, "int x = 7;", "int") == T_TYPE);
    hl_close((uint32_t)doc);
}

static void t_leaks(void)
{
    uint32_t live = 0u;
    uint32_t round;
    uint32_t i;
    int doc;

    puts("a hundred documents opened, edited, painted and closed, twice: the second run keeps nothing");

    for (round = 0u; round < 2u; round++)
    {
        for (i = 0u; i < 100u; i++)
        {
            doc = open_text(L_C, sample);
            edit_text(doc, i, 0u, "/* x */ int y = FOO; ");
            id_of(doc, sample, "printf");
            hl_close((uint32_t)doc);
        }

        CHECK(!round || hosted_port_live == live);
        live = hosted_port_live;
    }
}

int main(void)
{
    t_regex();
    t_open();
    t_predicates();
    t_neovim();
    t_inner();
    t_late_parse();
    t_find();
    t_refused();
    t_flatten();
    t_utf16();
    t_edit();
    t_plain();
    t_bad_arguments();
    t_fuzz_valid();
    t_fuzz_broken();
    t_many();
    t_no_memory();
    t_leaks();
    t_late();
    printf("%d checks, %d failed\n", checks, fails);

    return fails ? 1 : 0;
}
