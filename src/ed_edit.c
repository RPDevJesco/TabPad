#include "ed_int.h"

#define INDENT_MAX 256u
#define INDENT_BLANKS 4u

uint16_t ed_unit(uint32_t off)
{
    uint32_t n;
    const uint16_t *run = hl_text(ed_now->doc, off, &n);

    return n ? run[0] : 0u;
}

static uint32_t end(void)
{
    return hl_len(ed_now->doc);
}

static int is_high(uint32_t u)
{
    return u >= 0xD800u && u <= 0xDBFFu;
}

static int is_low(uint32_t u)
{
    return u >= 0xDC00u && u <= 0xDFFFu;
}

static int is_blank(uint32_t u)
{
    return u == ' ' || u == '\t';
}

static int class_of(uint32_t u)
{
    if (is_blank(u) || u == '\n')
    {
        return 0;
    }

    return (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_' || u > 0x7Fu ? 1 : 2;
}

static uint32_t next(uint32_t off)
{
    if (off >= end())
    {
        return off;
    }

    return is_high(ed_unit(off)) && is_low(ed_unit(off + 1u)) ? off + 2u : off + 1u;
}

static uint32_t prev(uint32_t off)
{
    if (!off)
    {
        return 0u;
    }

    return off >= 2u && is_low(ed_unit(off - 1u)) && is_high(ed_unit(off - 2u)) ? off - 2u : off - 1u;
}

static uint32_t word_left(uint32_t off)
{
    int kind;

    while (off && !class_of(ed_unit(off - 1u)))
    {
        off--;
    }

    kind = off ? class_of(ed_unit(off - 1u)) : 0;

    while (off && class_of(ed_unit(off - 1u)) == kind)
    {
        off--;
    }

    return off;
}

static uint32_t word_right(uint32_t off)
{
    int kind = off < end() ? class_of(ed_unit(off)) : 0;

    while (off < end() && kind && class_of(ed_unit(off)) == kind)
    {
        off++;
    }

    while (off < end() && !class_of(ed_unit(off)))
    {
        off++;
    }

    return off;
}

static uint32_t weight(uint32_t u)
{
    if (u < 0x80u)
    {
        return 1u;
    }

    return u < 0x800u || is_high(u) || is_low(u) ? 2u : 3u;
}

uint32_t ed_weigh(const uint16_t *text, uint32_t n)
{
    uint32_t bytes = 0u;
    uint32_t i;

    for (i = 0u; i < n; i++)
    {
        bytes += weight(text[i]);
    }

    return bytes;
}

static uint32_t weigh_range(uint32_t from, uint32_t to)
{
    const uint16_t *run;
    uint32_t bytes = 0u;
    uint32_t got;

    for (; from < to; from += got)
    {
        run = hl_text(ed_now->doc, from, &got);
        got = got > to - from ? to - from : got;
        bytes += ed_weigh(run, got);
    }

    return bytes;
}

uint32_t ed_bytes_before(uint32_t off)
{
    struct ed_tab *t = ed_now;

    if (off >= t->mark_off)
    {
        t->mark_bytes += weigh_range(t->mark_off, off);
    }

    else
    {
        t->mark_bytes -= weigh_range(off, t->mark_off);
    }

    t->mark_off = off;

    return t->mark_bytes;
}

static uint32_t row_of(uint32_t off)
{
    uint32_t row = 0u;
    uint32_t col;

    hl_row_col(ed_now->doc, off, &row, &col);

    return row;
}

static uint32_t row_start(uint32_t row)
{
    uint32_t off = 0u;

    hl_row_start(ed_now->doc, row, &off);

    return off;
}

static uint32_t row_end(uint32_t row)
{
    return row + 1u < hl_rows(ed_now->doc) ? row_start(row + 1u) - 1u : end();
}


uint32_t ed_sel_from(void)
{
    return ed_now->caret < ed_now->anchor ? ed_now->caret : ed_now->anchor;
}

uint32_t ed_sel_to(void)
{
    return ed_now->caret > ed_now->anchor ? ed_now->caret : ed_now->anchor;
}

void ed_go(uint32_t off, int keep)
{
    ed_now->caret = off;

    if (!keep)
    {
        ed_now->anchor = off;
    }

    ed_now->want_x = -1;
    ed_now->undo.typing = 0;
    ed_reveal();
}

static void sel(uint32_t from, uint32_t to)
{
    ed_go(to, 0);
    ed_now->anchor = from;
}

static void rows_touched(uint32_t *first, uint32_t *last)
{
    *first = row_of(ed_sel_from());
    *last = row_of(ed_sel_to());

    if (*last > *first && ed_sel_to() == row_start(*last))
    {
        *last -= 1u;
    }
}

static void go_rows(int32_t by, int keep)
{
    int32_t x = ed_now->want_x < 0 ? ed_x_of(ed_now->caret) : ed_now->want_x;

    ed_go(ed_off_lines(ed_now->caret, by, x), keep);
    ed_now->want_x = x;
}

static uint32_t home(void)
{
    uint32_t start = row_start(row_of(ed_now->caret));
    uint32_t line = ed_line_start(ed_now->caret);
    uint32_t off = start;

    if (line > start && ed_now->caret != line)
    {
        return line;
    }

    while (off < end() && is_blank(ed_unit(off)))
    {
        off++;
    }

    return ed_now->caret == off ? start : off;
}

static uint32_t line_end(void)
{
    uint32_t stop = ed_line_end(ed_now->caret);

    return ed_now->caret == stop ? row_end(row_of(ed_now->caret)) : stop;
}

static uint32_t left_of(int keep, int word)
{
    if (!keep && !word && ed_sel_from() != ed_sel_to())
    {
        return ed_sel_from();
    }

    return word ? word_left(ed_now->caret) : prev(ed_now->caret);
}

static uint32_t right_of(int keep, int word)
{
    if (!keep && !word && ed_sel_from() != ed_sel_to())
    {
        return ed_sel_to();
    }

    return word ? word_right(ed_now->caret) : next(ed_now->caret);
}

void ed_select_word(uint32_t off)
{
    uint32_t from = off;
    uint32_t to = off;
    int kind = off < end() ? class_of(ed_unit(off)) : 0;

    while (from && ed_unit(from - 1u) != '\n' && class_of(ed_unit(from - 1u)) == kind)
    {
        from--;
    }

    while (to < end() && ed_unit(to) != '\n' && class_of(ed_unit(to)) == kind)
    {
        to++;
    }

    sel(from, to);
}

void ed_select_row(uint32_t off)
{
    uint32_t row = row_of(off);

    sel(row_start(row), row + 1u < hl_rows(ed_now->doc) ? row_start(row + 1u) : end());
}

static void copy_out(uint32_t from, uint32_t n, uint16_t *out)
{
    const uint16_t *run;
    uint32_t got;
    uint32_t off;
    uint32_t i;

    for (off = 0u; off < n; off += got)
    {
        run = hl_text(ed_now->doc, from + off, &got);
        got = got > n - off ? n - off : got;

        for (i = 0u; i < got; i++)
        {
            out[off + i] = run[i];
        }
    }
}

static int apply(uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n)
{
    struct ed_tab *t = ed_now;
    uint32_t before = ed_bytes_before(at);
    uint32_t out = weigh_range(at, at + removed);

    if (hl_edit(t->doc, at, removed, ins, n))
    {
        return -1;
    }

    t->bytes = t->bytes - out + ed_weigh(ins, n);
    t->mark_bytes = before;
    sess_edited(ed_cur);

    return 0;
}

int ed_replace(uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n, int typed)
{
    struct ed_tab *t = ed_now;
    struct ed_step *s = 0;
    uint32_t i;
    int joined;

    if (!removed && !n)
    {
        return 0;
    }

    if (typed && !removed && n == 1u)
    {
        s = ed_undo_join(&t->undo, at, ins[0]);
    }

    joined = s != 0;

    if (!s)
    {
        s = ed_undo_push(&t->undo, removed, n);
    }

    if (s && !joined)
    {
        s->at = at;
        s->caret_before = t->caret;
        s->anchor_before = t->anchor;
        copy_out(at, removed, s->text);

        for (i = 0u; i < n; i++)
        {
            s->text[removed + i] = ins[i];
        }
    }

    if (apply(at, removed, ins, n))
    {
        if (joined)
        {
            s->inserted--;
        }

        else if (s)
        {
            ed_undo_pop(&t->undo);
        }

        return -1;
    }

    if (!s)
    {
        ed_undo_free(&t->undo);
    }

    t->caret = at + n;
    t->anchor = t->caret;
    t->want_x = -1;
    t->undo.typing = typed && s != 0;

    if (s)
    {
        s->caret_after = t->caret;
    }

    ed_reveal();

    return 0;
}

static void insert(const uint16_t *text, uint32_t n, int typed)
{
    ed_replace(ed_sel_from(), ed_sel_to() - ed_sel_from(), text, n, typed);
}

static void erase(uint32_t other)
{
    uint32_t from = ed_now->caret < other ? ed_now->caret : other;
    uint32_t to = ed_now->caret > other ? ed_now->caret : other;

    if (ed_sel_from() != ed_sel_to())
    {
        from = ed_sel_from();
        to = ed_sel_to();
    }

    if (from != to)
    {
        ed_replace(from, to - from, 0, 0u, 0);
    }
}

static void enter(void)
{
    uint16_t text[INDENT_MAX + 1u];
    uint32_t off = row_start(row_of(ed_sel_from()));
    uint32_t n = 1u;

    text[0] = '\n';

    while (off < ed_sel_from() && n <= INDENT_MAX && is_blank(ed_unit(off)))
    {
        text[n++] = ed_unit(off++);
    }

    insert(text, n, 0);
}

static void undo(void)
{
    struct ed_tab *t = ed_now;
    struct ed_step *s;

    if (!t->undo.pos)
    {
        return;
    }

    s = &t->undo.steps[t->undo.pos - 1u];

    if (apply(s->at, s->inserted, s->text, s->removed))
    {
        return;
    }

    t->undo.pos--;
    ed_go(s->caret_before, 0);
    t->anchor = s->anchor_before;
}

static void redo(void)
{
    struct ed_tab *t = ed_now;
    struct ed_step *s;

    if (t->undo.pos == t->undo.n)
    {
        return;
    }

    s = &t->undo.steps[t->undo.pos];

    if (apply(s->at, s->removed, s->text + s->removed, s->inserted))
    {
        return;
    }

    t->undo.pos++;
    ed_go(s->caret_after, 0);
}

static void copy(void)
{
    uint8_t *bytes;
    uint32_t n;

    if (ed_sel_from() == ed_sel_to() || ed_encode(ed_now->doc, ed_sel_from(), ed_sel_to(), ED_ENC_UTF8, ed_eol_default, &bytes, &n))
    {
        return;
    }

    ed_clip_put((const char *)bytes, n);
    ed_mem_free(bytes);
}

static void paste(void)
{
    uint32_t n;
    const char *utf8 = ed_clip_get(&n);
    uint16_t *text;
    uint32_t len;
    uint32_t unused = 0u;

    if (!utf8 || !n || ed_decode((const uint8_t *)utf8, n, 0, &text, &len, &unused, &unused))
    {
        return;
    }

    insert(text, len, 0);
    ed_mem_free(text);
}

static uint16_t *lift(uint32_t from, uint32_t to, uint32_t spare)
{
    uint16_t *text = ed_mem_alloc(((size_t)(to - from) + spare + 1u) * sizeof *text);

    if (text)
    {
        copy_out(from, to - from, text);
    }

    return text;
}

static void duplicate(void)
{
    uint32_t caret = ed_now->caret;
    uint32_t anchor = ed_now->anchor;
    uint32_t first;
    uint32_t last;
    uint32_t from;
    uint32_t to;
    uint16_t *text;

    rows_touched(&first, &last);
    from = row_start(first);
    to = row_end(last);
    text = ed_mem_alloc(((size_t)(to - from) + 2u) * sizeof *text);

    if (!text)
    {
        return;
    }

    text[0] = '\n';
    copy_out(from, to - from, text + 1);
    ed_replace(to, 0u, text, to - from + 1u, 0);
    ed_mem_free(text);
    ed_go(caret + (to - from) + 1u, 0);
    ed_now->anchor = anchor + (to - from) + 1u;
}

static void delete_rows(void)
{
    uint32_t first;
    uint32_t last;
    uint32_t from;
    uint32_t to;

    rows_touched(&first, &last);
    from = row_start(first);
    to = last + 1u < hl_rows(ed_now->doc) ? row_start(last + 1u) : end();

    if (to == end() && from)
    {
        from--;
    }

    if (from != to)
    {
        ed_replace(from, to - from, 0, 0u, 0);
    }
}

static void move_rows(int down)
{
    uint32_t caret = ed_now->caret;
    uint32_t anchor = ed_now->anchor;
    uint32_t first;
    uint32_t last;
    uint32_t from;
    uint32_t mid;
    uint32_t to;
    uint32_t shift;
    uint32_t i;
    uint16_t *text;
    uint16_t *swapped;

    rows_touched(&first, &last);

    if (down ? last + 1u >= hl_rows(ed_now->doc) : !first)
    {
        return;
    }

    from = row_start(down ? first : first - 1u);
    mid = row_end(down ? last : first - 1u);
    to = row_end(down ? last + 1u : last);
    text = lift(from, to, 0u);
    swapped = lift(from, to, 0u);

    if (text && swapped)
    {
        for (i = 0u; i < to - mid - 1u; i++)
        {
            swapped[i] = text[mid - from + 1u + i];
        }

        swapped[i] = '\n';

        for (i = 0u; i < mid - from; i++)
        {
            swapped[to - mid + i] = text[i];
        }

        shift = down ? to - mid : mid - from + 1u;
        ed_replace(from, to - from, swapped, to - from, 0);
        ed_go(down ? caret + shift : caret - shift, 0);
        ed_now->anchor = down ? anchor + shift : anchor - shift;
    }

    ed_mem_free(text);
    ed_mem_free(swapped);
}

static void indent(int out)
{
    uint32_t first;
    uint32_t last;
    uint32_t from;
    uint32_t to;
    uint32_t n = 0u;
    uint32_t i;
    uint32_t skip;
    uint16_t *text;
    uint16_t *done;

    rows_touched(&first, &last);
    from = row_start(first);
    to = row_end(last);
    text = lift(from, to, 0u);
    done = ed_mem_alloc(((size_t)(to - from) + (last - first) + 2u) * sizeof *done);

    for (i = 0u; text && done && i <= to - from; i++)
    {
        if (!i || text[i - 1u] == '\n')
        {
            if (!out)
            {
                done[n++] = '\t';
            }

            for (skip = 0u; out && skip < INDENT_BLANKS && i < to - from && text[i] == ' '; skip++)
            {
                i++;
            }

            if (out && !skip && i < to - from && text[i] == '\t')
            {
                i++;
            }
        }

        if (i < to - from)
        {
            done[n++] = text[i];
        }
    }

    if (text && done && (n != to - from))
    {
        ed_replace(from, to - from, done, n, 0);
        sel(from, from + n);
    }

    ed_mem_free(text);
    ed_mem_free(done);
}

static void recase(int upper)
{
    uint32_t from = ed_sel_from();
    uint32_t to = ed_sel_to();
    uint32_t i;
    uint16_t *text = lift(from, to, 0u);
    uint16_t u;

    if (!text)
    {
        return;
    }

    for (i = 0u; i < to - from; i++)
    {
        u = text[i];

        if (upper && ((u >= 'a' && u <= 'z') || (u >= 0xE0u && u <= 0xFEu && u != 0xF7u)))
        {
            text[i] = (uint16_t)(u - 32u);
        }

        else if (!upper && ((u >= 'A' && u <= 'Z') || (u >= 0xC0u && u <= 0xDEu && u != 0xD7u)))
        {
            text[i] = (uint16_t)(u + 32u);
        }
    }

    ed_replace(from, to - from, text, to - from, 0);
    ed_mem_free(text);
    sel(from, to);
}

static void trim(void)
{
    uint32_t caret_row = row_of(ed_now->caret);
    uint32_t len = end();
    uint32_t n = 0u;
    uint32_t kept = 0u;
    uint32_t i;
    uint16_t *text = lift(0u, len, 0u);

    if (!text)
    {
        return;
    }

    for (i = 0u; i < len; i++)
    {
        if (text[i] == '\n')
        {
            n = kept;
        }

        text[n++] = text[i];

        if (!is_blank(text[i]))
        {
            kept = n;
        }
    }

    n = kept;

    if (n != len)
    {
        ed_replace(0u, len, text, n, 0);
        ed_go(row_end(caret_row), 0);
    }

    ed_mem_free(text);
}

void ed_edit_run(uint32_t cmd)
{
    switch (cmd)
    {
        case ED_CMD_UNDO: undo(); break;
        case ED_CMD_REDO: redo(); break;
        case ED_CMD_COPY: copy(); break;
        case ED_CMD_PASTE: paste(); break;
        case ED_CMD_DELETE: erase(ed_now->caret); break;
        case ED_CMD_SELECT_ALL: sel(0u, end()); break;
        case ED_CMD_DUP_LINE: duplicate(); break;
        case ED_CMD_DEL_LINE: delete_rows(); break;
        case ED_CMD_LINE_UP: move_rows(0); break;
        case ED_CMD_LINE_DOWN: move_rows(1); break;
        case ED_CMD_INDENT: indent(0); break;
        case ED_CMD_UNINDENT: indent(1); break;
        case ED_CMD_UPPER: recase(1); break;
        case ED_CMD_LOWER: recase(0); break;
        case ED_CMD_TRIM: trim(); break;

        case ED_CMD_CUT:
            copy();
            erase(ed_now->caret);
            break;

        default:
            break;
    }
}

void ed_edit_key(uint32_t key, uint32_t mods)
{
    static const uint16_t tab = '\t';
    struct ed_tab *t = ed_now;
    uint32_t first;
    uint32_t last;
    int keep = (mods & ED_MOD_SHIFT) != 0u;
    int word = (mods & ED_MOD_CTRL) != 0u;
    int32_t page = (int32_t)ed_rows_shown() - 1;

    switch (key)
    {
        case ED_KEY_LEFT:
            ed_go(left_of(keep, word), keep);
            break;

        case ED_KEY_RIGHT:
            ed_go(right_of(keep, word), keep);
            break;

        case ED_KEY_UP:
            go_rows(-1, keep);
            break;

        case ED_KEY_DOWN:
            go_rows(1, keep);
            break;

        case ED_KEY_PAGE_UP:
            go_rows(page > 0 ? -page : -1, keep);
            break;

        case ED_KEY_PAGE_DOWN:
            go_rows(page > 0 ? page : 1, keep);
            break;

        case ED_KEY_HOME:
            ed_go(word ? 0u : home(), keep);
            break;

        case ED_KEY_END:
            ed_go(word ? end() : line_end(), keep);
            break;

        case ED_KEY_BACKSPACE:
            erase(word ? word_left(t->caret) : prev(t->caret));
            break;

        case ED_KEY_DELETE:
            erase(word ? word_right(t->caret) : next(t->caret));
            break;

        case ED_KEY_ENTER:
            enter();
            break;

        case ED_KEY_TAB:
            rows_touched(&first, &last);

            if (last > first)
            {
                indent(0);
            }

            else
            {
                insert(&tab, 1u, 0);
            }

            break;

        default:
            break;
    }
}

void ed_edit_text(const uint16_t *text, uint32_t n)
{
    struct ed_tab *t = ed_now;

    if (ed_opt.overwrite && t->caret == t->anchor && t->caret < end() && ed_unit(t->caret) != '\n')
    {
        ed_replace(t->caret, next(t->caret) - t->caret, text, n, 0);
        return;
    }

    insert(text, n, n == 1u);
}
