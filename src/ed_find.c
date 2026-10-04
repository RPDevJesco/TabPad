#include "ed_int.h"

#define FIELD 128u
#define PATTERN (FIELD * 3u + 32u)
#define W_NONE 0u
#define W_FIND 1u
#define W_WITH 2u
#define W_NEXT 3u
#define W_PREV 4u
#define W_CASE 5u
#define W_WORD 6u
#define W_REPLACE 7u
#define W_ALL 8u
#define W_GO 9u
#define W_SHUT 10u
#define W_REGEX 11u

struct field {
    uint16_t text[FIELD];
    uint32_t n, caret;
};

struct reader {
    const uint16_t *run;
    uint32_t from, n;
};

static uint32_t kind;
static struct field fields[2];
static uint32_t focus = ED_NONE;
static uint32_t note = ED_NONE;
static uint32_t note_count;
static int bad;
static uint32_t groups[HL_GROUPS * 2u];

static uint16_t unit(struct reader *r, uint32_t off)
{
    if (off - r->from >= r->n)
    {
        r->run = hl_text(ed_now->doc, off, &r->n);
        r->from = off;
    }

    return r->run[off - r->from];
}

static uint32_t fold(uint32_t u)
{
    return !ed_opt.find_case && u >= 'A' && u <= 'Z' ? u + 32u : u;
}

static int in_word(uint32_t u)
{
    return (u >= '0' && u <= '9') || (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || u == '_' || u > 0x7Fu;
}

static int stands_at(struct reader *r, uint32_t off)
{
    const struct field *f = &fields[0];
    uint32_t len = hl_len(ed_now->doc);
    uint32_t i;

    if (off > len || f->n > len - off)
    {
        return 0;
    }

    for (i = 0u; i < f->n; i++)
    {
        if (fold(unit(r, off + i)) != fold(f->text[i]))
        {
            return 0;
        }
    }

    if (!ed_opt.find_word)
    {
        return 1;
    }

    return !(off && in_word(unit(r, off - 1u))) && !(off + f->n < len && in_word(unit(r, off + f->n)));
}

static uint32_t put(char *out, uint32_t k, const char *lit)
{
    while (*lit)
    {
        out[k++] = *lit++;
    }

    return k;
}

static uint32_t pattern(char *out)
{
    uint32_t k = 0u;

    k = put(out, k, ed_opt.find_case ? "" : "(?i)");
    k = put(out, k, ed_opt.find_word ? "\\b(?:" : "");
    k += ed_units_utf8(fields[0].text, fields[0].n, out + k, FIELD * 3u + 4u);
    k = put(out, k, ed_opt.find_word ? ")\\b" : "");

    return k;
}

static int find_in(uint32_t from, uint32_t to, uint32_t *at, uint32_t *n)
{
    struct reader r = { 0, 0u, 0u };
    char text[PATTERN];
    uint32_t off;
    int got;

    if (ed_opt.find_regex)
    {
        got = hl_find(ed_now->doc, text, pattern(text), from, to, at, n, groups);
        bad = got < 0;

        return got > 0;
    }

    for (off = from; off < to; off++)
    {
        if (stands_at(&r, off))
        {
            *at = off;
            *n = fields[0].n;

            return 1;
        }
    }

    return 0;
}

static int find_last(uint32_t from, uint32_t to, uint32_t *at, uint32_t *n)
{
    struct reader r = { 0, 0u, 0u };
    uint32_t off = from;
    uint32_t a;
    uint32_t k;
    int found = 0;

    if (ed_opt.find_regex)
    {
        while (find_in(off, to, &a, &k))
        {
            *at = a;
            *n = k;
            off = a + 1u;
            found = 1;
        }

        return found;
    }

    for (off = to; off > from; off--)
    {
        if (stands_at(&r, off - 1u))
        {
            *at = off - 1u;
            *n = fields[0].n;

            return 1;
        }
    }

    return 0;
}

static int look(uint32_t from, int back, uint32_t *at, uint32_t *n, int *wrapped)
{
    uint32_t len = hl_len(ed_now->doc);

    *wrapped = 0;
    bad = 0;

    if (!fields[0].n)
    {
        return 0;
    }

    if (back ? find_last(0u, from, at, n) : find_in(from, len + 1u, at, n))
    {
        return 1;
    }

    *wrapped = 1;

    return !bad && (back ? find_last(from, len + 1u, at, n) : find_in(0u, from, at, n));
}

static void seek(uint32_t from, int back)
{
    uint32_t at;
    uint32_t n;
    int wrapped;

    note_count = 0u;

    if (!look(from, back, &at, &n, &wrapped))
    {
        note = bad ? ED_W_N_BAD_PATTERN : fields[0].n ? ED_W_N_NOT_FOUND : ED_NONE;
        return;
    }

    note = wrapped ? ED_W_N_WRAPPED : ED_NONE;
    ed_go(at, 0);
    ed_go(at + n, 1);
}

static int on_match(void)
{
    uint32_t at;
    uint32_t n;

    return fields[0].n && ed_sel_to() > ed_sel_from() && find_in(ed_sel_from(), ed_sel_from() + 1u, &at, &n) && at == ed_sel_from() && n == ed_sel_to() - ed_sel_from();
}

static uint32_t expand(struct reader *r, uint16_t *out)
{
    const struct field *f = &fields[1];
    uint32_t k = 0u;
    uint32_t off;
    uint32_t i;
    uint16_t u;
    uint16_t next;

    for (i = 0u; i < f->n; i++)
    {
        u = f->text[i];
        next = i + 1u < f->n ? f->text[i + 1u] : 0u;

        if (ed_opt.find_regex && u == '$' && next >= '0' && next <= '9')
        {
            off = groups[(next - '0') * 2u];

            for (; off != ED_NONE && off < groups[(next - '0') * 2u + 1u]; off++, k++)
            {
                if (out)
                {
                    out[k] = unit(r, off);
                }
            }

            i++;
            continue;
        }

        if (ed_opt.find_regex && (u == '$' || u == '\\') && next == u)
        {
            i++;
        }

        else if (ed_opt.find_regex && u == '\\' && (next == 'n' || next == 't'))
        {
            u = next == 'n' ? '\n' : '\t';
            i++;
        }

        if (out)
        {
            out[k] = u;
        }

        k++;
    }

    return k;
}

static void replace_one(void)
{
    struct reader r = { 0, 0u, 0u };
    uint16_t *text;
    uint32_t n;

    if (on_match())
    {
        n = expand(&r, 0);
        text = ed_mem_alloc(((size_t)n + 1u) * sizeof *text);

        if (!text)
        {
            note = ED_W_N_NO_MEMORY;
            return;
        }

        expand(&r, text);
        ed_replace(ed_sel_from(), ed_sel_to() - ed_sel_from(), text, n, 0);
        ed_mem_free(text);
    }

    seek(ed_sel_to(), 0);
}

static void replace_all(void)
{
    struct reader r = { 0, 0u, 0u };
    uint32_t len = hl_len(ed_now->doc);
    uint32_t first = ED_NONE;
    uint32_t last = 0u;
    uint32_t count = 0u;
    uint32_t off = 0u;
    uint32_t at;
    uint32_t n;
    uint32_t k = 0u;
    size_t room = 1u;
    uint16_t *text;

    bad = 0;

    while (fields[0].n && find_in(off, len + 1u, &at, &n))
    {
        first = first == ED_NONE ? at : first;
        last = at + n;
        off = last;
        room += expand(&r, 0);
        count++;
    }

    note = ED_W_N_REPLACED;
    note_count = count;

    if (!count)
    {
        note = bad ? ED_W_N_BAD_PATTERN : ED_W_N_NOT_FOUND;
        return;
    }

    text = ed_mem_alloc(((size_t)(last - first) + room) * sizeof *text);

    if (!text)
    {
        note = ED_W_N_NO_MEMORY;
        note_count = 0u;
        return;
    }

    for (off = first; off < last; off = at + n)
    {
        if (!find_in(off, last, &at, &n))
        {
            at = last;
            n = 0u;
        }

        for (; off < at; off++)
        {
            text[k++] = unit(&r, off);
        }

        k += n ? expand(&r, text + k) : 0u;
    }

    ed_replace(first, last - first, text, k, 0);
    ed_mem_free(text);
}

static void go_to_line(void)
{
    const struct field *f = &fields[0];
    uint32_t rows = hl_rows(ed_now->doc);
    uint32_t line = 0u;
    uint32_t off = 0u;
    uint32_t i;

    for (i = 0u; i < f->n && line < 100000000u; i++)
    {
        line = line * 10u + (uint32_t)(f->text[i] - '0');
    }

    if (!f->n)
    {
        return;
    }

    line = line < 1u ? 1u : line;
    hl_row_start(ed_now->doc, line > rows ? rows - 1u : line - 1u, &off);
    ed_go(off, 0);
    ed_find_close();
}


static void field_put(struct field *f, const uint16_t *text, uint32_t n)
{
    uint32_t i;
    uint32_t k;

    for (i = 0u; i < n && f->n < FIELD; i++)
    {
        if (text[i] < 0x20u || (kind == ED_CMD_GOTO && (text[i] < '0' || text[i] > '9')))
        {
            continue;
        }

        for (k = f->n; k > f->caret; k--)
        {
            f->text[k] = f->text[k - 1u];
        }

        f->text[f->caret++] = text[i];
        f->n++;
    }
}

static void field_cut(struct field *f, uint32_t at)
{
    uint32_t k;

    if (at >= f->n)
    {
        return;
    }

    for (k = at; k + 1u < f->n; k++)
    {
        f->text[k] = f->text[k + 1u];
    }

    f->n--;
    f->caret = at;
}

static void changed(void)
{
    if (focus == 0u && kind != ED_CMD_GOTO)
    {
        seek(ed_sel_from(), 0);
    }
}

static void paste(struct field *f)
{
    uint32_t n;
    const char *utf8 = ed_clip_get(&n);
    uint16_t text[FIELD];

    if (utf8)
    {
        field_put(f, text, ed_utf8_units(utf8, n, text, FIELD));
    }
}

struct walk {
    int draw;
    int32_t mx, my;
    uint32_t found;
    int32_t x, y;
};

static int32_t row_height(void)
{
    return ed_g.ui_lh + ed_px(10);
}

static int thing(struct walk *w, uint32_t id, int32_t width, uint32_t back)
{
    int32_t h = ed_g.ui_lh + ed_px(4);

    if (w->draw)
    {
        ed_draw_rect(w->x, w->y, width, h, ed_colours[ED_C_BORDER]);
        ed_draw_rect(w->x + 1, w->y + 1, width - 2, h - 2, ed_colours[back]);
    }

    else if (w->mx >= w->x && w->mx < w->x + width && w->my >= w->y && w->my < w->y + h)
    {
        w->found = id;
    }

    w->x += width + ED_PAD;

    return w->draw;
}

static void button(struct walk *w, uint32_t id, const char *label, int on)
{
    int32_t x = w->x;

    if (thing(w, id, ed_say_width(label) + ED_PAD * 2, on ? ED_C_MENU_HOT : ED_C_MENU_BACK))
    {
        ed_say(x + ED_PAD, w->y + ed_px(2), label, ed_colours[ED_C_MENU_TEXT]);
    }
}

static void label(struct walk *w, const char *text, int32_t width)
{
    if (w->draw)
    {
        ed_say(w->x, w->y + ed_px(2), text, ed_colours[ED_C_MENU_TEXT]);
    }

    w->x += width;
}

static void field(struct walk *w, uint32_t id, uint32_t which, int32_t width)
{
    const struct field *f = &fields[which];
    int32_t x = w->x;
    int32_t caret = ed_text_width(f->text, f->caret);
    int32_t slide = caret > width - ED_PAD * 2 ? caret - width + ED_PAD * 2 : 0;

    if (!thing(w, id, width, ED_C_FIELD_BACK))
    {
        return;
    }

    ed_draw_clip(x + ed_px(2), w->y, width - ed_px(4), ed_g.ui_lh + ed_px(4));
    ed_draw_text(x + ED_PAD - slide, w->y + ed_px(2), f->text, f->n, ed_colours[ED_C_TEXT]);

    if (focus == which)
    {
        ed_draw_rect(x + ED_PAD - slide + caret, w->y + ed_px(2), ed_px(2), ed_g.ui_lh, ed_colours[ED_C_CARET]);
    }

    ed_draw_clip(0, ed_g.panel_y, ed_w, ed_g.panel_h);
}

static void say_note(struct walk *w)
{
    char text[96];
    const char *words = note == ED_NONE ? "" : ed_word(note);
    uint32_t v = note_count;
    uint32_t n = 0u;
    uint32_t i;
    char back[10];

    while (v)
    {
        back[n++] = (char)('0' + v % 10u);
        v /= 10u;
    }

    for (i = 0u; i < n; i++)
    {
        text[i] = back[n - 1u - i];
    }

    text[n] = n ? ' ' : 0;
    n += n ? 1u : 0u;

    for (i = 0u; words[i] && n < 95u; i++)
    {
        text[n++] = words[i];
    }

    text[n] = 0;
    ed_say(w->x, w->y + ed_px(2), text, ed_colours[ED_C_STATUS_WARN]);
}

static uint32_t walk(int draw, int32_t mx, int32_t my)
{
    struct walk w = { 0, 0, 0, W_NONE, 0, 0 };
    const char *first = ed_word(kind == ED_CMD_GOTO ? ED_W_L_GOTO : ED_W_L_FIND);
    int32_t names = ed_say_width(first);
    int32_t wide = ed_w / 4 < ed_px(160) ? ed_px(160) : ed_w / 4;

    names = ed_say_width(ed_word(ED_W_L_REPLACE)) > names && kind != ED_CMD_GOTO ? ed_say_width(ed_word(ED_W_L_REPLACE)) : names;
    names += ED_PAD;
    w.draw = draw;
    w.mx = mx;
    w.my = my;
    w.x = ED_PAD;
    w.y = ed_g.panel_y + ed_px(4);
    label(&w, first, names);

    if (kind == ED_CMD_GOTO)
    {
        field(&w, W_FIND, 0u, ed_px(120));
        button(&w, W_GO, ed_word(ED_W_B_GO), 0);
    }

    else
    {
        field(&w, W_FIND, 0u, wide);
        button(&w, W_NEXT, ed_word(ED_W_B_NEXT), 0);
        button(&w, W_PREV, ed_word(ED_W_B_PREV), 0);
        button(&w, W_CASE, ed_word(ED_W_B_CASE), ed_opt.find_case);
        button(&w, W_WORD, ed_word(ED_W_B_WORD), ed_opt.find_word);
        button(&w, W_REGEX, ed_word(ED_W_B_REGEX), ed_opt.find_regex);
    }

    if (draw)
    {
        say_note(&w);
    }

    w.x = ed_w - ED_PAD - ed_say_width("x") - ED_PAD * 2;
    button(&w, W_SHUT, "x", 0);

    if (kind == ED_CMD_REPLACE)
    {
        w.x = ED_PAD;
        w.y += row_height();
        label(&w, ed_word(ED_W_L_REPLACE), names);
        field(&w, W_WITH, 1u, wide);
        button(&w, W_REPLACE, ed_word(ED_W_B_REPLACE), 0);
        button(&w, W_ALL, ed_word(ED_W_B_ALL), 0);
    }

    return w.found;
}

static void press(uint32_t id)
{
    switch (id)
    {
        case W_FIND: focus = 0u; break;
        case W_WITH: focus = 1u; break;
        case W_NEXT: ed_find_next(0); break;
        case W_PREV: ed_find_next(1); break;
        case W_REPLACE: replace_one(); break;
        case W_ALL: replace_all(); break;
        case W_GO: go_to_line(); break;
        case W_SHUT: ed_find_close(); break;

        case W_CASE:
            ed_opt.find_case = !ed_opt.find_case;
            seek(ed_sel_from(), 0);
            break;

        case W_WORD:
            ed_opt.find_word = !ed_opt.find_word;
            seek(ed_sel_from(), 0);
            break;

        case W_REGEX:
            ed_opt.find_regex = !ed_opt.find_regex;
            seek(ed_sel_from(), 0);
            break;

        default:
            break;
    }
}

void ed_find_open(uint32_t what)
{
    uint32_t from = ed_sel_from();
    uint32_t n = ed_sel_to() - from;
    uint32_t i;

    kind = what;
    focus = 0u;
    note = ED_NONE;
    note_count = 0u;

    if (what == ED_CMD_GOTO)
    {
        fields[0].n = 0u;
        fields[0].caret = 0u;
        return;
    }

    for (i = 0u; i < n && n < FIELD && ed_unit(from + i) != '\n'; i++)
    {
    }

    if (n && i == n)
    {
        for (i = 0u; i < n; i++)
        {
            fields[0].text[i] = ed_unit(from + i);
        }

        fields[0].n = n;
    }

    fields[0].caret = fields[0].n;
}

void ed_find_close(void)
{
    kind = 0u;
    focus = ED_NONE;
    ed_touch();
}

int32_t ed_find_height(int32_t ui_lh)
{
    if (!kind)
    {
        return 0;
    }

    return (ui_lh + ed_px(10)) * (kind == ED_CMD_REPLACE ? 2 : 1) + ed_px(4);
}

int ed_find_focus(void)
{
    return kind && focus != ED_NONE;
}

void ed_find_blur(void)
{
    focus = ED_NONE;
}

int ed_find_ready(void)
{
    return fields[0].n > 0u && kind != ED_CMD_GOTO;
}

void ed_find_next(int back)
{
    seek(back ? ed_sel_from() : ed_sel_to(), back);
    ed_touch();
}

void ed_find_text(const uint16_t *text, uint32_t n)
{
    field_put(&fields[focus], text, n);
    changed();
    ed_touch();
}

int ed_find_key(uint32_t key, uint32_t mods)
{
    struct field *f = &fields[focus];

    switch (key)
    {
        case ED_KEY_ESCAPE:
            ed_find_close();
            break;

        case ED_KEY_LEFT:
            f->caret -= f->caret ? 1u : 0u;
            break;

        case ED_KEY_RIGHT:
            f->caret += f->caret < f->n ? 1u : 0u;
            break;

        case ED_KEY_HOME:
            f->caret = 0u;
            break;

        case ED_KEY_END:
            f->caret = f->n;
            break;

        case ED_KEY_BACKSPACE:
            if (f->caret)
            {
                field_cut(f, f->caret - 1u);
                changed();
            }

            break;

        case ED_KEY_DELETE:
            field_cut(f, f->caret);
            changed();
            break;

        case ED_KEY_TAB:
            focus = kind == ED_CMD_REPLACE ? focus ^ 1u : focus;
            break;

        case ED_KEY_ENTER:
            if (kind == ED_CMD_GOTO)
            {
                go_to_line();
            }

            else if (focus == 1u)
            {
                replace_one();
            }

            else
            {
                ed_find_next((mods & ED_MOD_SHIFT) != 0u);
            }

            break;

        default:
            if (key != 'v' || mods != ED_MOD_CTRL)
            {
                return 0;
            }

            paste(f);
            changed();
            break;
    }

    ed_touch();

    return 1;
}

int ed_find_down(int32_t x, int32_t y)
{
    if (!kind || y < ed_g.panel_y || y >= ed_g.panel_y + ed_g.panel_h)
    {
        return 0;
    }

    ed_text_font(ED_FONT_UI, 100);
    focus = focus == ED_NONE ? 0u : focus;
    press(walk(0, x, y));
    ed_touch();

    return 1;
}

void ed_find_draw(void)
{
    if (!kind)
    {
        return;
    }

    ed_text_font(ED_FONT_UI, 100);
    ed_draw_clip(0, ed_g.panel_y, ed_w, ed_g.panel_h);
    ed_draw_rect(0, ed_g.panel_y, ed_w, ed_g.panel_h, ed_colours[ED_C_MENU_BACK]);
    ed_draw_rect(0, ed_g.panel_y, ed_w, 1, ed_colours[ED_C_BORDER]);
    walk(1, 0, 0);
}
