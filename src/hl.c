#include "hl_int.h"
#include <tree_sitter/api.h>

#define NONE 0xFFFFFFFFu
#define DOCS_FIRST 8u
#define CAPS_FIRST 256u
#define RANGES_FIRST 64u
#define KEPT 64u
#define LAYER 0x100000u
#define BUDGET 200u

struct doc {
    struct hl_buf buf;
    TSTree *tree;
    uint32_t lang;
    int used;
    uint32_t anc_off, anc_row;
    uint32_t rows;
    uint32_t dirty_from, dirty_to;
    TSTree *inner[HL_INNER_MAX];
    int late;
};

struct kept {
    TSTree *tree;
    uint32_t doc, layer, from, to;
};

struct cap {
    uint32_t from, to, id, pat;
    uint32_t up;
};

static struct doc *docs;
static uint32_t doc_max;
static TSParser *parser;
static TSQueryCursor *cursor;
static TSQueryCursor *seeker;
static struct cap *caps;
static uint32_t cap_max;
static TSRange *ranges;
static uint32_t range_max;
static struct kept kept[KEPT];
static uint32_t kept_next;
static uint32_t steps;
static uint32_t steps_most;

static struct doc *get(uint32_t doc)
{
    return doc < doc_max && docs[doc].used ? &docs[doc] : 0;
}

static int slot(void)
{
    struct doc *grown;
    uint32_t max;
    uint32_t i;

    for (i = 0u; i < doc_max; i++)
    {
        if (!docs[i].used)
        {
            return (int)i;
        }
    }

    max = doc_max ? doc_max * 2u : DOCS_FIRST;
    grown = hl_mem_alloc((size_t)max * sizeof *grown);

    if (!grown)
    {
        return -1;
    }

    for (i = 0u; i < doc_max; i++)
    {
        grown[i] = docs[i];
    }

    for (i = doc_max; i < max; i++)
    {
        grown[i].used = 0;
    }

    hl_mem_free(docs);
    docs = grown;
    i = doc_max;
    doc_max = max;

    return (int)i;
}

static const char *feed(void *payload, uint32_t byte, TSPoint point, uint32_t *bytes)
{
    const struct doc *d = payload;
    const uint16_t *run;
    uint32_t n;

    (void)point;
    run = hl_buf_run(&d->buf, byte / 2u, &n);
    *bytes = n * 2u;

    return (const char *)run;
}

static TSInputEncoding encoding(void)
{
    const uint16_t one = 1u;

    return *(const uint8_t *)&one ? TSInputEncodingUTF16LE : TSInputEncodingUTF16BE;
}

static bool watch(TSParseState *state)
{
    (void)state;
    steps++;

    return steps_most && steps > steps_most;
}

static TSTree *parse(const struct doc *d, uint32_t lang, uint32_t n, const TSTree *old, uint32_t most)
{
    TSParseOptions options;
    TSTree *tree;
    TSInput in;

    if (!ts_parser_set_language(parser, hl_langs[lang].grammar()) || !ts_parser_set_included_ranges(parser, ranges, n))
    {
        return 0;
    }

    in.payload = (void *)(uintptr_t)d;
    in.read = feed;
    in.encoding = encoding();
    in.decode = 0;
    options.payload = 0;
    options.progress_callback = watch;
    steps = 0u;
    steps_most = most;
    tree = ts_parser_parse_with_options(parser, old, in, options);

    if (!tree)
    {
        ts_parser_reset(parser);
    }

    return tree;
}

static uint32_t row_at(struct doc *d, uint32_t off)
{
    uint32_t row = d->anc_row;
    uint32_t i;

    for (i = d->anc_off; i < off; i++)
    {
        if (hl_buf_at(&d->buf, i) == '\n')
        {
            row++;
        }
    }

    for (i = off; i < d->anc_off; i++)
    {
        if (hl_buf_at(&d->buf, i) == '\n')
        {
            row--;
        }
    }

    d->anc_off = off;
    d->anc_row = row;

    return row;
}

static uint32_t col_at(const struct doc *d, uint32_t off)
{
    uint32_t i = off;

    while (i && hl_buf_at(&d->buf, i - 1u) != '\n')
    {
        i--;
    }

    return off - i;
}

static uint32_t start_of(struct doc *d, uint32_t row)
{
    uint32_t off = d->anc_off;
    uint32_t r = d->anc_row;

    while (r < row)
    {
        if (hl_buf_at(&d->buf, off) == '\n')
        {
            r++;
        }

        off++;
    }

    while (off)
    {
        if (hl_buf_at(&d->buf, off - 1u) == '\n')
        {
            if (r == row)
            {
                break;
            }

            r--;
        }

        off--;
    }

    d->anc_off = off;
    d->anc_row = row;

    return off;
}

static void step(TSPoint *p, uint16_t unit)
{
    if (unit == '\n')
    {
        p->row++;
        p->column = 0u;
        return;
    }

    p->column += 2u;
}

static void mark(struct doc *d, uint32_t from, uint32_t to)
{
    if (d->dirty_from == d->dirty_to)
    {
        d->dirty_from = from;
        d->dirty_to = to;
        return;
    }

    d->dirty_from = from < d->dirty_from ? from : d->dirty_from;
    d->dirty_to = to > d->dirty_to ? to : d->dirty_to;
}

static uint32_t moved(uint32_t off, uint32_t at, uint32_t removed, uint32_t n, int end)
{
    if (off <= at)
    {
        return off;
    }

    if (off >= at + removed)
    {
        return off - removed + n;
    }

    return end ? at + n : at;
}

static void mark_token(struct doc *d, TSNode root, uint32_t a, uint32_t b)
{
    TSNode node = ts_node_descendant_for_byte_range(root, a * 2u, b * 2u);

    if (ts_node_is_null(node) || ts_node_child_count(node))
    {
        return;
    }

    mark(d, ts_node_start_byte(node) / 2u, ts_node_end_byte(node) / 2u);
}

static void mark_edit(struct doc *d, const TSTree *fresh, uint32_t at, uint32_t n)
{
    TSNode root = ts_tree_root_node(fresh);
    TSRange *changed;
    uint32_t count;
    uint32_t to = at + n;
    uint32_t i;

    mark(d, at, to);
    mark_token(d, root, at ? at - 1u : at, at);
    mark_token(d, root, to, to < d->buf.len ? to + 1u : to);
    changed = ts_tree_get_changed_ranges(d->tree, fresh, &count);

    for (i = 0u; i < count; i++)
    {
        mark(d, changed[i].start_byte / 2u, changed[i].end_byte / 2u);
    }

    hl_c_free(changed);
    d->dirty_to = d->dirty_to > d->buf.len ? d->buf.len : d->dirty_to;
}

static int range_room(uint32_t n)
{
    TSRange *grown;
    uint32_t max;
    uint32_t i;

    if (n < range_max)
    {
        return 1;
    }

    max = range_max ? range_max * 2u : RANGES_FIRST;
    grown = hl_mem_alloc((size_t)max * sizeof *grown);

    if (!grown)
    {
        return 0;
    }

    for (i = 0u; i < n; i++)
    {
        grown[i] = ranges[i];
    }

    hl_mem_free(ranges);
    ranges = grown;
    range_max = max;

    return 1;
}

static uint32_t carve(TSNode node, uint32_t n)
{
    TSNode child;
    TSRange r;
    uint32_t count = ts_node_named_child_count(node);
    uint32_t i;

    r.start_byte = ts_node_start_byte(node);
    r.start_point = ts_node_start_point(node);

    for (i = 0u; i <= count; i++)
    {
        child = i < count ? ts_node_named_child(node, i) : node;
        r.end_byte = i < count ? ts_node_start_byte(child) : ts_node_end_byte(node);
        r.end_point = i < count ? ts_node_start_point(child) : ts_node_end_point(node);

        if (r.end_byte > r.start_byte && range_room(n))
        {
            ranges[n++] = r;
        }

        r.start_byte = ts_node_end_byte(child);
        r.start_point = ts_node_end_point(child);
    }

    return n;
}

static void sort_ranges(uint32_t n)
{
    TSRange r;
    uint32_t i;
    uint32_t k;

    for (i = 1u; i < n; i++)
    {
        r = ranges[i];

        for (k = i; k && ranges[k - 1u].start_byte > r.start_byte; k--)
        {
            ranges[k] = ranges[k - 1u];
        }

        ranges[k] = r;
    }
}

static void forget(uint32_t doc)
{
    uint32_t i;

    for (i = 0u; i < KEPT; i++)
    {
        if (kept[i].tree && kept[i].doc == doc)
        {
            ts_tree_delete(kept[i].tree);
            kept[i].tree = 0;
        }
    }
}

static const TSTree *lone(const struct doc *d, uint32_t layer, uint32_t lang, TSNode node)
{
    struct kept *e;
    uint32_t doc = (uint32_t)(d - docs);
    uint32_t from = ts_node_start_byte(node);
    uint32_t to = ts_node_end_byte(node);
    uint32_t n;
    uint32_t i;

    for (i = 0u; i < KEPT; i++)
    {
        e = &kept[i];

        if (e->tree && e->doc == doc && e->layer == layer && e->from == from && e->to == to)
        {
            return e->tree;
        }
    }

    n = carve(node, 0u);
    e = &kept[kept_next];

    if (!n)
    {
        return 0;
    }

    if (e->tree)
    {
        ts_tree_delete(e->tree);
    }

    e->tree = parse(d, lang, n, 0, 0u);
    e->doc = doc;
    e->layer = layer;
    e->from = from;
    e->to = to;
    kept_next = (kept_next + 1u) % KEPT;

    return e->tree;
}

static void mark_tree(struct doc *d, const TSTree *tree)
{
    TSNode root = ts_tree_root_node(tree);

    mark(d, ts_node_start_byte(root) / 2u, ts_node_end_byte(root) / 2u);
}

static int captured(const TSQueryMatch *m, uint32_t capture, TSNode *node)
{
    uint32_t i;

    for (i = 0u; i < m->capture_count; i++)
    {
        if (m->captures[i].index == capture)
        {
            *node = m->captures[i].node;

            return 1;
        }
    }

    return 0;
}

static int named_by(const char *names, const struct hl_buf *b, uint32_t at, uint32_t len)
{
    uint32_t i;
    uint32_t c;

    while (names && *names)
    {
        for (i = 0u; names[i] && names[i] != ' '; i++)
        {
        }

        for (c = 0u; c < i && c < len; c++)
        {
            if ((hl_buf_at(b, at + c) | 0x20u) != ((uint8_t)names[c] | 0x20u))
            {
                break;
            }
        }

        if (c == i && i == len)
        {
            return 1;
        }

        names += names[i] ? i + 1u : i;
    }

    return 0;
}

static uint32_t lang_named(const struct doc *d, TSNode name)
{
    uint32_t at = ts_node_start_byte(name) / 2u;
    uint32_t len = ts_node_end_byte(name) / 2u - at;
    uint32_t i;

    for (i = 0u; i < hl_lang_count; i++)
    {
        if (named_by(hl_langs[i].names, &d->buf, at, len))
        {
            return i;
        }
    }

    return NONE;
}

static void join(struct doc *d, const struct hl_ql *l, uint32_t layer, uint32_t lang)
{
    TSQueryMatch m;
    TSNode node;
    TSRange *changed;
    TSTree *fresh = 0;
    uint32_t count;
    uint32_t n = 0u;
    uint32_t i;

    ts_query_cursor_set_byte_range(seeker, 0u, NONE);
    ts_query_cursor_exec(seeker, l->inner[layer], ts_tree_root_node(d->tree));

    while (ts_query_cursor_next_match(seeker, &m))
    {
        if (captured(&m, l->inner_i[layer], &node))
        {
            n = carve(node, n);
        }
    }

    sort_ranges(n);

    if (n)
    {
        fresh = parse(d, lang, n, d->inner[layer], 0u);
    }

    if (fresh && d->inner[layer])
    {
        changed = ts_tree_get_changed_ranges(d->inner[layer], fresh, &count);

        for (i = 0u; i < count; i++)
        {
            mark(d, changed[i].start_byte / 2u, changed[i].end_byte / 2u);
        }

        hl_c_free(changed);
    }

    else if (fresh)
    {
        mark_tree(d, fresh);
    }

    if (d->inner[layer])
    {
        if (!fresh)
        {
            mark_tree(d, d->inner[layer]);
        }

        ts_tree_delete(d->inner[layer]);
    }

    d->inner[layer] = fresh;
}

static void mark_lone(struct doc *d, const struct hl_ql *l, uint32_t layer, uint32_t from, uint32_t to)
{
    TSQueryMatch m;
    TSNode node;

    ts_query_cursor_set_byte_range(seeker, from * 2u, to * 2u);
    ts_query_cursor_exec(seeker, l->inner[layer], ts_tree_root_node(d->tree));

    while (ts_query_cursor_next_match(seeker, &m))
    {
        if (captured(&m, l->inner_i[layer], &node))
        {
            mark(d, ts_node_start_byte(node) / 2u, ts_node_end_byte(node) / 2u);
        }
    }
}

static void layers(struct doc *d, uint32_t from, uint32_t to)
{
    const struct hl_lang *lang = &hl_langs[d->lang];
    const struct hl_ql *l;
    uint32_t i;

    for (i = 0u; i < lang->inner_count && i < HL_INNER_MAX; i++)
    {
        if (lang->inner[i].lang != HL_NAMED && !hl_q_get(lang->inner[i].lang))
        {
            continue;
        }

        l = hl_q_get(d->lang);

        if (!l->inner[i])
        {
            continue;
        }

        if (lang->inner[i].joined)
        {
            join(d, l, i, lang->inner[i].lang);
        }

        else if (from < to)
        {
            mark_lone(d, l, i, from, to);
        }
    }

    d->dirty_to = d->dirty_to > d->buf.len ? d->buf.len : d->dirty_to;
}

static int do_edit(struct doc *d, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    TSInputEdit e;
    TSTree *fresh;
    uint32_t i;

    e.start_byte = at * 2u;
    e.old_end_byte = (at + removed) * 2u;
    e.new_end_byte = (at + n) * 2u;
    e.start_point.row = row_at(d, at);
    e.start_point.column = col_at(d, at) * 2u;
    e.old_end_point = e.start_point;
    e.new_end_point = e.start_point;

    for (i = 0u; i < removed; i++)
    {
        step(&e.old_end_point, hl_buf_at(&d->buf, at + i));
    }

    if (hl_buf_edit(&d->buf, at, removed, ins, n))
    {
        return -1;
    }

    for (i = 0u; i < n; i++)
    {
        step(&e.new_end_point, ins[i]);
    }

    d->rows += e.new_end_point.row - e.old_end_point.row;
    d->dirty_from = moved(d->dirty_from, at, removed, n, 0);
    d->dirty_to = moved(d->dirty_to, at, removed, n, 1);

    if (d->lang == HL_PLAIN)
    {
        mark(d, at, at + n);

        return 0;
    }

    forget((uint32_t)(d - docs));
    ts_tree_edit(d->tree, &e);

    for (i = 0u; i < HL_INNER_MAX; i++)
    {
        if (d->inner[i])
        {
            ts_tree_edit(d->inner[i], &e);
        }
    }

    fresh = d->late ? 0 : parse(d, d->lang, 0u, d->tree, BUDGET);

    if (!fresh)
    {
        d->late = 1;
        mark(d, at, at + n);

        return 0;
    }

    mark_edit(d, fresh, at, n);
    ts_tree_delete(d->tree);
    d->tree = fresh;
    layers(d, at ? at - 1u : at, at + n + 1u);

    return 0;
}

static void settle(struct doc *d)
{
    TSTree *fresh = parse(d, d->lang, 0u, d->tree, 0u);
    uint32_t from = d->dirty_from;
    uint32_t to = d->dirty_to;

    d->late = 0;

    if (!fresh)
    {
        mark(d, 0u, d->buf.len);
        return;
    }

    mark_edit(d, fresh, from, to - from);
    ts_tree_delete(d->tree);
    d->tree = fresh;
    layers(d, from ? from - 1u : from, to + 1u);
}

static int do_open(uint32_t lang, const uint16_t *text, uint32_t len)
{
    struct doc *d;
    uint32_t i;
    int id;

    if (lang != HL_PLAIN && !hl_q_get(lang))
    {
        return -1;
    }

    if (lang != HL_PLAIN && !parser)
    {
        parser = ts_parser_new();
        cursor = ts_query_cursor_new();
        seeker = ts_query_cursor_new();
    }

    id = slot();

    if (id < 0)
    {
        return -1;
    }

    d = &docs[id];

    if (hl_buf_open(&d->buf, text, len))
    {
        return -1;
    }

    d->lang = lang;
    d->anc_off = 0u;
    d->anc_row = 0u;
    d->rows = 1u;
    d->tree = lang == HL_PLAIN ? 0 : parse(d, lang, 0u, 0, 0u);
    d->late = 0;

    for (i = 0u; i < HL_INNER_MAX; i++)
    {
        d->inner[i] = 0;
    }

    if (lang != HL_PLAIN && !d->tree)
    {
        hl_buf_close(&d->buf);

        return -1;
    }

    for (i = 0u; i < len; i++)
    {
        if (text[i] == '\n')
        {
            d->rows++;
        }
    }

    if (lang != HL_PLAIN)
    {
        layers(d, 0u, 0u);
    }

    d->dirty_from = 0u;
    d->dirty_to = 0u;
    d->used = 1;

    return id;
}

static int cap_room(uint32_t n)
{
    struct cap *grown;
    uint32_t max;
    uint32_t i;

    if (n < cap_max)
    {
        return 1;
    }

    max = cap_max ? cap_max * 2u : CAPS_FIRST;
    grown = hl_mem_alloc((size_t)max * sizeof *grown);

    if (!grown)
    {
        return 0;
    }

    for (i = 0u; i < n; i++)
    {
        grown[i] = caps[i];
    }

    hl_mem_free(caps);
    caps = grown;
    cap_max = max;

    return 1;
}

static uint32_t collect(const struct hl_buf *b, const TSTree *tree, const struct hl_ql *l, uint32_t from, uint32_t to, uint32_t n, uint32_t base)
{
    TSQueryMatch m;
    TSQueryCapture c;
    struct cap *cap;
    uint32_t index;

    ts_query_cursor_set_byte_range(cursor, from * 2u, to * 2u);
    ts_query_cursor_exec(cursor, l->q, ts_tree_root_node(tree));

    while (ts_query_cursor_next_capture(cursor, &m, &index))
    {
        if (!hl_q_pass(l, b, &m))
        {
            ts_query_cursor_remove_match(cursor, m.id);
            continue;
        }

        c = m.captures[index];

        if (ts_node_end_byte(c.node) / 2u > b->len || l->theme[c.index] == HL_NONE || ts_node_start_byte(c.node) / 2u >= to || ts_node_end_byte(c.node) / 2u <= from)
        {
            continue;
        }

        if (!cap_room(n))
        {
            break;
        }

        cap = &caps[n++];
        cap->from = ts_node_start_byte(c.node) / 2u;
        cap->to = ts_node_end_byte(c.node) / 2u;
        cap->id = l->theme[c.index];
        cap->pat = base + m.pattern_index;
    }

    return n;
}

static uint32_t collect_lone(const struct doc *d, uint32_t layer, uint32_t from, uint32_t to, uint32_t n)
{
    const struct hl_inner *in = &hl_langs[d->lang].inner[layer];
    const struct hl_ql *il;
    const struct hl_ql *l;
    const TSTree *tree;
    TSQueryMatch m;
    TSNode node;
    TSNode name;
    uint32_t lang;

    l = hl_q_get(d->lang);
    ts_query_cursor_set_byte_range(seeker, from * 2u, to * 2u);
    ts_query_cursor_exec(seeker, l->inner[layer], ts_tree_root_node(d->tree));

    while (ts_query_cursor_next_match(seeker, &m))
    {
        lang = in->lang;

        if (!captured(&m, l->inner_i[layer], &node))
        {
            continue;
        }

        if (lang == HL_NAMED)
        {
            lang = captured(&m, l->inner_n[layer], &name) ? lang_named(d, name) : NONE;
        }

        il = lang == NONE ? 0 : hl_q_get(lang);
        l = hl_q_get(d->lang);
        tree = il ? lone(d, layer, lang, node) : 0;

        if (tree)
        {
            n = collect(&d->buf, tree, il, from, to, n, LAYER * (layer + 1u));
        }
    }

    return n;
}

static uint32_t collect_all(const struct doc *d, uint32_t from, uint32_t to)
{
    const struct hl_lang *lang = &hl_langs[d->lang];
    const struct hl_ql *il;
    const struct hl_ql *l;
    uint32_t n;
    uint32_t i;

    n = collect(&d->buf, d->tree, hl_q_get(d->lang), from, to, 0u, 0u);

    for (i = 0u; i < lang->inner_count && i < HL_INNER_MAX; i++)
    {
        l = hl_q_get(d->lang);

        if (!l->inner[i])
        {
            continue;
        }

        if (!lang->inner[i].joined)
        {
            n = collect_lone(d, i, from, to, n);
            continue;
        }

        il = hl_q_get(lang->inner[i].lang);

        if (il && d->inner[i])
        {
            n = collect(&d->buf, d->inner[i], il, from, to, n, LAYER * (i + 1u));
        }
    }

    return n;
}

static int before(const struct cap *a, const struct cap *b)
{
    if (a->from != b->from)
    {
        return a->from < b->from;
    }

    if (a->to != b->to)
    {
        return a->to > b->to;
    }

    return a->pat <= b->pat;
}

static void order(uint32_t n)
{
    struct cap c;
    uint32_t i;
    uint32_t k;

    for (i = 1u; i < n; i++)
    {
        c = caps[i];

        for (k = i; k && !before(&caps[k - 1u], &c); k--)
        {
            caps[k] = caps[k - 1u];
        }

        caps[k] = c;
    }
}

struct sink {
    uint32_t *out;
    uint32_t from, to, max, count;
};

static void emit(struct sink *s, uint32_t a, uint32_t b, uint32_t id)
{
    uint32_t *span;

    a = a < s->from ? s->from : a;
    b = b > s->to ? s->to : b;

    if (a >= b || s->count == s->max)
    {
        return;
    }

    span = s->out + s->count * HL_SPAN_WORDS;
    span[HL_SPAN_AT] = a;
    span[HL_SPAN_LEN] = b - a;
    span[HL_SPAN_ID] = id;
    s->count++;
}

static void sweep(struct sink *s, uint32_t n)
{
    uint32_t top = NONE;
    uint32_t pos = s->from;
    uint32_t i;

    for (i = 0u; i <= n; i++)
    {
        while (top != NONE && (i == n || caps[top].to <= caps[i].from))
        {
            emit(s, pos, caps[top].to, caps[top].id);
            pos = caps[top].to > pos ? caps[top].to : pos;
            top = caps[top].up;
        }

        if (i == n)
        {
            return;
        }

        if (top != NONE)
        {
            emit(s, pos, caps[i].from, caps[top].id);
        }

        pos = caps[i].from > pos ? caps[i].from : pos;

        if (top != NONE && caps[top].from == caps[i].from && caps[top].to == caps[i].to)
        {
            caps[top].id = caps[i].id;
            continue;
        }

        caps[i].up = top;
        top = i;
    }
}

int hl_open(uint32_t lang, const uint16_t *text, uint32_t len)
{
    if ((lang >= hl_lang_count && lang != HL_PLAIN) || len > HL_LEN_MAX || (!text && len))
    {
        return -1;
    }

    return do_open(lang, text, len);
}

void hl_close(uint32_t doc)
{
    struct doc *d = get(doc);
    uint32_t i;

    if (!d)
    {
        return;
    }

    if (d->tree)
    {
        ts_tree_delete(d->tree);
    }

    for (i = 0u; i < HL_INNER_MAX; i++)
    {
        if (d->inner[i])
        {
            ts_tree_delete(d->inner[i]);
        }
    }

    forget(doc);

    hl_buf_close(&d->buf);
    d->used = 0;
}

int hl_edit(uint32_t doc, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    struct doc *d = get(doc);

    if (!d || at > d->buf.len || removed > d->buf.len - at || (!ins && n))
    {
        return -1;
    }

    if (n > HL_LEN_MAX - (d->buf.len - removed))
    {
        return -1;
    }

    return do_edit(d, at, removed, ins, n);
}

uint32_t hl_spans(uint32_t doc, uint32_t from, uint32_t to, uint32_t *out, uint32_t max)
{
    const struct doc *d = get(doc);
    struct sink s;
    uint32_t n;

    if (!d || !out || d->lang == HL_PLAIN)
    {
        return 0u;
    }

    to = to > d->buf.len ? d->buf.len : to;

    if (from >= to)
    {
        return 0u;
    }

    s.out = out;
    s.from = from;
    s.to = to;
    s.max = max;
    s.count = 0u;
    n = collect_all(d, from, to);
    order(n);
    sweep(&s, n);

    return s.count;
}

int hl_dirty(uint32_t doc, uint32_t *from, uint32_t *to)
{
    struct doc *d = get(doc);

    if (!d || d->dirty_from == d->dirty_to)
    {
        return 0;
    }

    *from = d->dirty_from;
    *to = d->dirty_to;
    d->dirty_from = 0u;
    d->dirty_to = 0u;

    return 1;
}

uint32_t hl_len(uint32_t doc)
{
    const struct doc *d = get(doc);

    return d ? d->buf.len : 0u;
}

const uint16_t *hl_text(uint32_t doc, uint32_t off, uint32_t *n)
{
    const struct doc *d = get(doc);

    if (!d)
    {
        *n = 0u;

        return 0;
    }

    return hl_buf_run(&d->buf, off, n);
}

int hl_row_col(uint32_t doc, uint32_t off, uint32_t *row, uint32_t *col)
{
    struct doc *d = get(doc);

    if (!d || off > d->buf.len)
    {
        return -1;
    }

    *row = row_at(d, off);
    *col = col_at(d, off);

    return 0;
}

uint32_t hl_rows(uint32_t doc)
{
    const struct doc *d = get(doc);

    return d ? d->rows : 0u;
}

int hl_row_start(uint32_t doc, uint32_t row, uint32_t *off)
{
    struct doc *d = get(doc);

    if (!d || row >= d->rows)
    {
        return -1;
    }

    *off = start_of(d, row);

    return 0;
}

int hl_find(uint32_t doc, const char *regex, uint32_t n, uint32_t from, uint32_t to, uint32_t *at, uint32_t *len, uint32_t *groups)
{
    const struct doc *d = get(doc);

    if (!d || !regex || !hl_re_ok(regex, n))
    {
        return -1;
    }

    return hl_re_find(regex, n, &d->buf, from, to, at, len, groups);
}

uint32_t hl_settle(void)
{
    uint32_t count = 0u;
    uint32_t i;

    for (i = 0u; i < doc_max; i++)
    {
        if (docs[i].used && docs[i].late)
        {
            forget(i);
            settle(&docs[i]);
            count++;
        }
    }

    return count;
}
