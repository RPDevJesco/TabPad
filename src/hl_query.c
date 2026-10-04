#include "hl_int.h"
#include <tree_sitter/api.h>

enum op { OP_EQ, OP_MATCH, OP_ANY_OF, OP_LUA, OP_HAS, OP_UNDER, OP_PARENT, OP_SKIP, OP_NEVER, OP_BAD };

struct pred {
    const char *name;
    enum op op;
    int neg, any;
};

static const struct pred preds[] = {
    { "eq?", OP_EQ, 0, 0 },
    { "not-eq?", OP_EQ, 1, 0 },
    { "any-eq?", OP_EQ, 0, 1 },
    { "any-not-eq?", OP_EQ, 1, 1 },
    { "match?", OP_MATCH, 0, 0 },
    { "not-match?", OP_MATCH, 1, 0 },
    { "any-match?", OP_MATCH, 0, 1 },
    { "any-not-match?", OP_MATCH, 1, 1 },
    { "any-of?", OP_ANY_OF, 0, 0 },
    { "not-any-of?", OP_ANY_OF, 1, 0 },
    { "lua-match?", OP_LUA, 0, 0 },
    { "not-lua-match?", OP_LUA, 1, 0 },
    { "any-lua-match?", OP_LUA, 0, 1 },
    { "any-not-lua-match?", OP_LUA, 1, 1 },
    { "contains?", OP_HAS, 0, 0 },
    { "not-contains?", OP_HAS, 1, 0 },
    { "any-contains?", OP_HAS, 0, 1 },
    { "any-not-contains?", OP_HAS, 1, 1 },
    { "has-ancestor?", OP_UNDER, 0, 0 },
    { "not-has-ancestor?", OP_UNDER, 1, 0 },
    { "has-parent?", OP_PARENT, 0, 0 },
    { "not-has-parent?", OP_PARENT, 1, 0 },
    { "is?", OP_NEVER, 0, 0 },
    { "is-not?", OP_NEVER, 0, 0 },
};

static struct hl_ql *table;
static uint32_t table_n;

static int name_eq(const char *s, uint32_t n, const char *lit)
{
    uint32_t i;

    for (i = 0u; i < n; i++)
    {
        if (!lit[i] || lit[i] != s[i])
        {
            return 0;
        }
    }

    return !lit[n];
}

static struct pred classify(const char *s, uint32_t n)
{
    struct pred other = { 0, OP_BAD, 0, 0 };
    uint32_t i;

    for (i = 0u; i < sizeof preds / sizeof preds[0]; i++)
    {
        if (name_eq(s, n, preds[i].name))
        {
            return preds[i];
        }
    }

    if (n && s[n - 1u] == '!')
    {
        other.op = OP_SKIP;
    }

    return other;
}

static int text_at(const struct hl_buf *b, uint32_t at, uint32_t end, const char *s, uint32_t n, uint32_t *stop)
{
    const char *e = s + n;
    uint32_t w;

    while (s < e)
    {
        if (at == end || hl_buf_cp(b, at, end, &w) != hl_utf8(&s, e))
        {
            return 0;
        }

        at += w;
    }

    *stop = at;

    return 1;
}

static int text_is(const struct hl_buf *b, uint32_t at, uint32_t len, const char *s, uint32_t n)
{
    uint32_t stop;

    return text_at(b, at, at + len, s, n, &stop) && stop == at + len;
}

static int text_has(const struct hl_buf *b, uint32_t at, uint32_t len, const char *s, uint32_t n)
{
    uint32_t stop;
    uint32_t i;

    for (i = 0u; i <= len; i++)
    {
        if (text_at(b, at + i, at + len, s, n, &stop))
        {
            return 1;
        }
    }

    return 0;
}

static int under(const struct hl_ql *l, TSNode node, const TSQueryPredicateStep *st, uint32_t n, int far)
{
    const char *s;
    uint32_t slen;
    uint32_t i;

    for (node = ts_node_parent(node); !ts_node_is_null(node); node = ts_node_parent(node))
    {
        for (i = 2u; i < n; i++)
        {
            s = ts_query_string_value_for_id(l->q, st[i].value_id, &slen);

            if (name_eq(s, slen, ts_node_type(node)))
            {
                return 1;
            }
        }

        if (!far)
        {
            break;
        }
    }

    return 0;
}

static uint32_t span_of(const char *s)
{
    uint32_t n = 0u;

    while (s[n])
    {
        n++;
    }

    return n;
}

static int text_same(const struct hl_buf *b, uint32_t at, uint32_t len, uint32_t at2, uint32_t len2)
{
    uint32_t i;

    if (len != len2)
    {
        return 0;
    }

    for (i = 0u; i < len; i++)
    {
        if (hl_buf_at(b, at + i) != hl_buf_at(b, at2 + i))
        {
            return 0;
        }
    }

    return 1;
}

static int first_node(const TSQueryMatch *m, uint32_t capture, uint32_t *at, uint32_t *len)
{
    uint32_t i;

    for (i = 0u; i < m->capture_count; i++)
    {
        if (m->captures[i].index == capture)
        {
            *at = ts_node_start_byte(m->captures[i].node) / 2u;
            *len = ts_node_end_byte(m->captures[i].node) / 2u - *at;

            return 1;
        }
    }

    return 0;
}

static int holds(const struct hl_ql *l, const struct hl_buf *b, const TSQueryMatch *m, enum op op, const TSQueryPredicateStep *st, uint32_t n, TSNode node)
{
    const char *s;
    uint32_t at = ts_node_start_byte(node) / 2u;
    uint32_t len = ts_node_end_byte(node) / 2u - at;
    uint32_t slen;
    uint32_t at2;
    uint32_t len2;
    uint32_t i;

    if (at + len > b->len)
    {
        return 0;
    }

    if (op == OP_EQ && st[2].type == TSQueryPredicateStepTypeCapture)
    {
        return !first_node(m, st[2].value_id, &at2, &len2) || (at2 + len2 <= b->len && text_same(b, at, len, at2, len2));
    }

    if (op == OP_UNDER || op == OP_PARENT)
    {
        return under(l, node, st, n, op == OP_UNDER);
    }

    if (op == OP_LUA)
    {
        s = l->lua[st[2].value_id];

        return hl_re_match(s, span_of(s), b, at, len);
    }

    for (i = 2u; i < n; i++)
    {
        s = ts_query_string_value_for_id(l->q, st[i].value_id, &slen);

        if (op == OP_MATCH ? hl_re_match(s, slen, b, at, len) : op == OP_HAS ? text_has(b, at, len, s, slen) : text_is(b, at, len, s, slen))
        {
            return 1;
        }
    }

    return 0;
}

static int decide(const struct hl_ql *l, const struct hl_buf *b, const TSQueryMatch *m, const TSQueryPredicateStep *st, uint32_t n)
{
    struct pred p;
    const char *s;
    uint32_t slen;
    uint32_t i;

    s = ts_query_string_value_for_id(l->q, st[0].value_id, &slen);
    p = classify(s, slen);

    if (p.op == OP_SKIP || p.op == OP_NEVER)
    {
        return p.op == OP_SKIP;
    }

    for (i = 0u; i < m->capture_count; i++)
    {
        if (m->captures[i].index != st[1].value_id)
        {
            continue;
        }

        if ((holds(l, b, m, p.op, st, n, m->captures[i].node) != p.neg) == p.any)
        {
            return p.any;
        }
    }

    return !p.any;
}

static uint32_t put(char *out, uint32_t k, const char *lit)
{
    while (*lit)
    {
        out[k++] = *lit++;
    }

    return k;
}

static const char *lua_set(char c)
{
    switch (c)
    {
        case 'a': return "A-Za-z";
        case 'd': return "\\d";
        case 'l': return "a-z";
        case 'u': return "A-Z";
        case 's': return "\\s";
        case 'w': return "A-Za-z0-9";
        case 'x': return "A-Fa-f0-9";
        case 'p': return "!-/:-@\\[-`{-~";
        case 'c': return "\x01-\x1f\x7f";
        case 'D': return "\\D";
        case 'S': return "\\S";
        default: return 0;
    }
}

static int lua_alnum(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int lua_class(char c, int in_set, char *out, uint32_t *k)
{
    const char *set = lua_set(c);
    char low = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);

    if (!lua_alnum(c))
    {
        if ((uint8_t)c < 0x80u)
        {
            out[(*k)++] = '\\';
        }

        out[(*k)++] = c;

        return 1;
    }

    if (set && (in_set || set[0] == '\\'))
    {
        *k = put(out, *k, set);

        return 1;
    }

    if (in_set)
    {
        return 0;
    }

    set = set ? set : low != c ? lua_set(low) : 0;

    if (!set)
    {
        return 0;
    }

    *k = put(out, *k, low != c ? "[^" : "[");
    *k = put(out, *k, set);
    *k = put(out, *k, "]");

    return 1;
}

static int lua_bracket(const char *s, uint32_t n, uint32_t *at, char *out, uint32_t *k)
{
    uint32_t i = *at + 1u;
    int first = 1;

    out[(*k)++] = '[';

    if (i < n && s[i] == '^')
    {
        out[(*k)++] = '^';
        i++;
    }

    while (i < n && (s[i] != ']' || first))
    {
        first = 0;

        if (s[i] == '%')
        {
            if (i + 1u == n || !lua_class(s[i + 1u], 1, out, k))
            {
                return 0;
            }

            i += 2u;
            continue;
        }

        if (s[i] == '[' || s[i] == ']' || s[i] == '\\')
        {
            out[(*k)++] = '\\';
        }

        out[(*k)++] = s[i++];
    }

    if (i == n)
    {
        return 0;
    }

    out[(*k)++] = ']';
    *at = i + 1u;

    return 1;
}

static int lua_plain(char c)
{
    return c == '^' || c == '$' || c == '|' || c == '{' || c == '}' || c == '\\';
}

static char *lua_regex(const char *s, uint32_t n)
{
    char *out = hl_mem_alloc((size_t)n * 10u + 8u);
    uint32_t k = 0u;
    uint32_t i = 0u;
    int atom = 0;
    int was;

    if (!out)
    {
        return 0;
    }

    if (n && s[0] == '^')
    {
        out[k++] = '^';
        i = 1u;
    }

    while (i < n)
    {
        was = atom;
        atom = 1;

        if (s[i] == '%')
        {
            if (i + 1u == n || !lua_class(s[i + 1u], 0, out, &k))
            {
                break;
            }

            i += 2u;
            continue;
        }

        if (s[i] == '[')
        {
            if (!lua_bracket(s, n, &i, out, &k))
            {
                break;
            }

            continue;
        }

        if (s[i] == '.')
        {
            k = put(out, k, "[\\s\\S]");
        }

        else if (s[i] == '$' && i + 1u == n)
        {
            out[k++] = '$';
        }

        else if (s[i] == '(' && i + 1u < n && s[i + 1u] == ')')
        {
            atom = 0;
            i++;
        }

        else if (s[i] == '(' || s[i] == ')')
        {
            atom = 0;
            out[k++] = s[i];
        }

        else if (was && (s[i] == '*' || s[i] == '+' || s[i] == '?' || s[i] == '-'))
        {
            atom = 0;
            out[k++] = s[i] == '-' ? '*' : s[i];
        }

        else
        {
            if (lua_plain(s[i]) || s[i] == '*' || s[i] == '+' || s[i] == '?')
            {
                out[k++] = '\\';
            }

            out[k++] = s[i];
        }

        i++;
    }

    out[k] = 0;

    if (i < n || !hl_re_ok(out, k))
    {
        hl_mem_free(out);

        return 0;
    }

    return out;
}

static int known(struct hl_ql *l, const TSQueryPredicateStep *st, uint32_t n)
{
    struct pred p;
    const char *s;
    uint32_t slen;
    uint32_t i;

    if (!n || st[0].type != TSQueryPredicateStepTypeString)
    {
        return 0;
    }

    s = ts_query_string_value_for_id(l->q, st[0].value_id, &slen);
    p = classify(s, slen);

    if (p.op == OP_SKIP || p.op == OP_NEVER)
    {
        return 1;
    }

    if (p.op == OP_BAD || n < 3u || st[1].type != TSQueryPredicateStepTypeCapture)
    {
        return 0;
    }

    if (p.op == OP_EQ && n == 3u && st[2].type == TSQueryPredicateStepTypeCapture)
    {
        return 1;
    }

    if ((p.op == OP_EQ || p.op == OP_MATCH || p.op == OP_LUA) && n != 3u)
    {
        return 0;
    }

    for (i = 2u; i < n; i++)
    {
        if (st[i].type != TSQueryPredicateStepTypeString)
        {
            return 0;
        }

        s = ts_query_string_value_for_id(l->q, st[i].value_id, &slen);

        if (p.op == OP_MATCH && !hl_re_ok(s, slen))
        {
            return 0;
        }

        if (p.op == OP_LUA && !l->lua[st[i].value_id])
        {
            l->lua[st[i].value_id] = lua_regex(s, slen);

            if (!l->lua[st[i].value_id])
            {
                return 0;
            }
        }
    }

    return 1;
}

static int all_known(struct hl_ql *l)
{
    const TSQueryPredicateStep *st;
    uint32_t count;
    uint32_t pat;
    uint32_t from;
    uint32_t i;

    for (pat = 0u; pat < ts_query_pattern_count(l->q); pat++)
    {
        st = ts_query_predicates_for_pattern(l->q, pat, &count);
        from = 0u;

        for (i = 0u; i < count; i++)
        {
            if (st[i].type != TSQueryPredicateStepTypeDone)
            {
                continue;
            }

            if (!known(l, st + from, i - from))
            {
                return 0;
            }

            from = i + 1u;
        }
    }

    return 1;
}

static uint32_t theme_of(const char *name, uint32_t n)
{
    uint32_t best = HL_NONE;
    uint32_t best_len = 0u;
    uint32_t i;
    uint32_t k;

    for (i = 0u; i < hl_theme_count; i++)
    {
        for (k = 0u; k < n && hl_theme[i][k] && hl_theme[i][k] == name[k]; k++)
        {
        }

        if (hl_theme[i][k] || (k < n && name[k] != '.'))
        {
            continue;
        }

        if (best == HL_NONE || k > best_len)
        {
            best = i;
            best_len = k;
        }
    }

    return best;
}

static void unload(struct hl_ql *l)
{
    uint32_t count = ts_query_string_count(l->q);
    uint32_t i;

    for (i = 0u; l->lua && i < count; i++)
    {
        if (l->lua[i])
        {
            hl_mem_free(l->lua[i]);
        }
    }

    if (l->lua)
    {
        hl_mem_free(l->lua);
    }

    if (l->theme)
    {
        hl_mem_free(l->theme);
    }

    ts_query_delete(l->q);
    l->lua = 0;
    l->theme = 0;
    l->q = 0;
}

static uint32_t capture_of(const TSQuery *q, const char *name)
{
    const char *got;
    uint32_t len;
    uint32_t i;

    for (i = 0u; i < ts_query_capture_count(q); i++)
    {
        got = ts_query_capture_name_for_id(q, i, &len);

        if (name_eq(got, len, name))
        {
            return i;
        }
    }

    return HL_NONE;
}

static void seek_query(struct hl_ql *l, const struct hl_lang *lang, uint32_t k)
{
    const struct hl_inner *in = &lang->inner[k];
    char text[160];
    TSQueryError err;
    uint32_t off;
    uint32_t n = 0u;

    l->inner[k] = 0;

    if (in->query)
    {
        l->inner[k] = ts_query_new(lang->grammar(), in->query, span_of(in->query), &off, &err);
    }

    else if (span_of(in->node) + (in->parent ? span_of(in->parent) : 0u) + 16u <= sizeof text)
    {
        n = put(text, n, in->parent ? "(" : "");
        n = put(text, n, in->parent ? in->parent : "");
        n = put(text, n, in->parent ? " (" : "(");
        n = put(text, n, in->node);
        n = put(text, n, in->parent ? ") @i)" : ") @i");
        l->inner[k] = ts_query_new(lang->grammar(), text, n, &off, &err);
    }

    if (!l->inner[k])
    {
        return;
    }

    l->inner_i[k] = capture_of(l->inner[k], "i");
    l->inner_n[k] = capture_of(l->inner[k], "n");

    if (l->inner_i[k] == HL_NONE || (in->lang == HL_NAMED && (l->inner_n[k] == HL_NONE || in->joined)))
    {
        ts_query_delete(l->inner[k]);
        l->inner[k] = 0;
    }
}

static void load(struct hl_ql *l, const struct hl_lang *lang)
{
    TSQueryError err;
    const char *name;
    uint32_t off;
    uint32_t count;
    uint32_t len = 0u;
    uint32_t i;

    while (lang->query[len])
    {
        len++;
    }

    l->state = 2;
    l->q = ts_query_new(lang->grammar(), lang->query, len, &off, &err);

    if (!l->q)
    {
        return;
    }

    count = ts_query_string_count(l->q);
    l->lua = hl_mem_alloc(((size_t)count + 1u) * sizeof *l->lua);
    count = ts_query_capture_count(l->q);
    l->theme = hl_mem_alloc(((size_t)count + 1u) * sizeof *l->theme);

    for (i = 0u; l->lua && i < ts_query_string_count(l->q); i++)
    {
        l->lua[i] = 0;
    }

    if (!l->lua || !l->theme || !all_known(l))
    {
        l->state = l->lua && l->theme ? 2 : 0;
        unload(l);

        return;
    }

    for (i = 0u; i < count; i++)
    {
        name = ts_query_capture_name_for_id(l->q, i, &len);
        l->theme[i] = theme_of(name, len);
    }

    for (i = 0u; i < HL_INNER_MAX; i++)
    {
        l->inner[i] = 0;

        if (i < lang->inner_count)
        {
            seek_query(l, lang, i);
        }
    }

    l->state = 1;
}

const struct hl_ql *hl_q_get(uint32_t lang)
{
    struct hl_ql *grown;
    uint32_t i;

    if (lang >= hl_lang_count)
    {
        return 0;
    }

    if (lang >= table_n)
    {
        grown = hl_mem_alloc((size_t)hl_lang_count * sizeof *grown);

        if (!grown)
        {
            return 0;
        }

        for (i = 0u; i < hl_lang_count; i++)
        {
            if (i < table_n)
            {
                grown[i] = table[i];
                continue;
            }

            grown[i].q = 0;
            grown[i].theme = 0;
            grown[i].lua = 0;
            grown[i].state = 0;
        }

        if (table)
        {
            hl_mem_free(table);
        }

        table = grown;
        table_n = hl_lang_count;
    }

    if (!table[lang].state)
    {
        load(&table[lang], &hl_langs[lang]);
    }

    return table[lang].state == 1 ? &table[lang] : 0;
}

int hl_lang_ok(uint32_t lang)
{
    return hl_q_get(lang) != 0;
}

int hl_q_pass(const struct hl_ql *l, const struct hl_buf *b, const TSQueryMatch *m)
{
    const TSQueryPredicateStep *st;
    uint32_t count;
    uint32_t from = 0u;
    uint32_t i;

    st = ts_query_predicates_for_pattern(l->q, m->pattern_index, &count);

    for (i = 0u; i < count; i++)
    {
        if (st[i].type != TSQueryPredicateStepTypeDone)
        {
            continue;
        }

        if (!decide(l, b, m, st + from, i - from))
        {
            return 0;
        }

        from = i + 1u;
    }

    return 1;
}
