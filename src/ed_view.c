#include "ed_int.h"

#define TAB 4u
#define PIECE 4096u
#define CELLS (PIECE * TAB + TAB + 1u)
#define SUBS 1024u
#define LAST 0xFFFFFFFFu
#define SPANS 256u
#define NAME 64u
#define PLAIN 0xFFu
#define FAINT 0xFEu
#define COLUMN 0xE0u
#define COLUMNS 16u

int32_t ed_w;
int32_t ed_h;
int32_t ed_scale = 100;
struct ed_bands ed_g;

struct line {
    uint16_t cells[CELLS];
    uint32_t off[CELLS + 1u];
    int32_t x[CELLS + 1u];
    uint8_t ink[CELLS];
    uint32_t brk[SUBS + 1u];
    uint32_t n, subs;
    uint32_t start, end;
    uint32_t from, to;
    uint32_t piece, pieces;
};

static struct line ln;
static int32_t bar_left;
static uint32_t seen_from;
static uint32_t seen_to;

int32_t ed_px(int32_t n)
{
    int32_t v = (n * ed_scale + 50) / 100;

    return n > 0 && v < 1 ? 1 : v;
}

int32_t ed_say_width(const char *utf8)
{
    uint16_t text[128];
    uint32_t n = 0u;

    while (utf8[n])
    {
        n++;
    }

    return ed_text_width(text, ed_utf8_units(utf8, n, text, 128u));
}

int32_t ed_say(int32_t x, int32_t y, const char *utf8, uint32_t rgb)
{
    uint16_t text[128];
    uint32_t n = 0u;

    while (utf8[n])
    {
        n++;
    }

    n = ed_utf8_units(utf8, n, text, 128u);
    ed_draw_text(x, y, text, n, rgb);

    return ed_text_width(text, n);
}

static uint32_t row_start(uint32_t row)
{
    uint32_t off = 0u;

    hl_row_start(ed_now->doc, row, &off);

    return off;
}

static uint32_t row_of(uint32_t off)
{
    uint32_t row = 0u;
    uint32_t col;

    hl_row_col(ed_now->doc, off, &row, &col);

    return row;
}

void ed_measure(void)
{
    uint32_t rows = ed_now && ed_now->loaded ? hl_rows(ed_now->doc) : 1u;
    char digits[12];
    uint32_t n = 0u;

    for (; rows || n < 3u; rows /= 10u)
    {
        digits[n++] = '0';
    }

    digits[n] = 0;
    ed_text_font(ED_FONT_UI, 100);
    ed_g.ui_lh = ed_line_height();
    ed_g.icon = (ED_ICON * ed_scale / 100 + 2) / 4 * 4;
    ed_g.icon = ed_g.icon < ED_ICON ? ED_ICON : ed_g.icon;
    ed_g.stroke = (ed_g.icon + ED_ICON - 1) / ED_ICON;
    ed_g.scroll_w = ed_px(12);
    ed_g.menu_h = ed_g.ui_lh + ed_px(8);
    ed_g.tool_y = ed_g.menu_h;
    ed_g.tool_h = ed_opt.tool ? ed_g.icon + ed_px(12) : 0;
    ed_g.bar_y = ed_g.tool_y + ed_g.tool_h;
    ed_g.bar_h = ed_g.ui_lh + ED_PAD * 2;
    ed_g.text_y = ed_g.bar_y + ed_g.bar_h;
    ed_g.status_h = ed_opt.status ? ed_g.ui_lh + ed_px(8) : 0;
    ed_g.status_y = ed_h - ed_g.status_h;
    ed_g.panel_h = ed_find_height(ed_g.ui_lh);
    ed_g.panel_y = ed_g.status_y - ed_g.panel_h;
    ed_text_font(ED_FONT_TEXT, ed_opt.zoom);
    ed_g.lh = ed_line_height();
    ed_g.lh = ed_g.lh < 1 ? 1 : ed_g.lh;
    ed_g.gutter_w = ed_opt.numbers ? ed_say_width(digits) + ED_PAD * 2 : 0;
    ed_g.text_x = ed_g.gutter_w + ED_PAD;
    ed_g.text_w = ed_w - ed_g.text_x - ed_g.scroll_w;
    ed_g.text_h = ed_g.panel_y - ed_g.text_y;
    ed_g.text_w = ed_g.text_w < 1 ? 1 : ed_g.text_w;
    ed_g.text_h = ed_g.text_h < ed_g.lh ? ed_g.lh : ed_g.text_h;
}

uint32_t ed_rows_shown(void)
{
    ed_measure();

    return (uint32_t)(ed_g.text_h / ed_g.lh);
}

static uint16_t picture(uint16_t unit, int first, int *faint)
{
    *faint = ed_opt.spaces && (unit == ' ' || unit == '\t');

    if (unit == '\t')
    {
        return *faint && first ? 0x2192u : ' ';
    }

    if (unit == ' ')
    {
        return *faint ? 0xB7u : ' ';
    }

    return unit < 0x20u || unit == 0x7Fu ? ' ' : unit;
}

static int is_high(uint32_t u)
{
    return u >= 0xD800u && u <= 0xDBFFu;
}

static int is_low(uint32_t u)
{
    return u >= 0xDC00u && u <= 0xDFFFu;
}

static int breaks_after(uint32_t c)
{
    return ln.cells[c] == ' ' || ln.ink[c] == FAINT;
}

static void place(void)
{
    uint32_t c;
    uint32_t w;

    ln.x[0] = 0;

    for (c = 0u; c < ln.n; c += w)
    {
        w = is_high(ln.cells[c]) && c + 1u < ln.n && is_low(ln.cells[c + 1u]) ? 2u : 1u;
        ln.x[c + 1u] = ln.x[c];
        ln.x[c + w] = ln.x[c] + ed_text_width(ln.cells + c, w);
    }
}

static uint32_t fit(uint32_t from, int32_t room)
{
    uint32_t lo = from;
    uint32_t hi = ln.n;
    uint32_t mid;

    while (lo < hi)
    {
        mid = (lo + hi + 1u) / 2u;

        if (ln.x[mid] - ln.x[from] <= room)
        {
            lo = mid;
        }

        else
        {
            hi = mid - 1u;
        }
    }

    return lo;
}

static void wrap(void)
{
    int32_t room = ed_g.text_w - ED_PAD;
    uint32_t from = 0u;
    uint32_t back;
    uint32_t c;

    ln.subs = 0u;
    ln.brk[0] = 0u;

    while (ed_opt.wrap && ln.subs + 1u < SUBS)
    {
        c = fit(from, room);

        if (c >= ln.n)
        {
            break;
        }

        c = c == from ? from + 1u : c;
        c += c < ln.n && is_low(ln.cells[c]) ? 1u : 0u;

        while (c < ln.n && breaks_after(c))
        {
            c++;
        }

        for (back = c; back > from && !breaks_after(back - 1u); back--)
        {
        }

        c = back > from ? back : c;

        if (c >= ln.n)
        {
            break;
        }

        ln.brk[++ln.subs] = c;
        from = c;
    }

    ln.brk[++ln.subs] = ln.n;
}

static uint16_t unit_at(uint32_t off)
{
    uint32_t n;
    const uint16_t *run = hl_text(ed_now->doc, off, &n);

    return n ? run[0] : 0u;
}

static uint32_t piece_from(uint32_t k)
{
    uint32_t off;

    if (k >= ln.pieces)
    {
        return ln.end;
    }

    off = ln.start + k * PIECE;

    return k && is_low(unit_at(off)) ? off + 1u : off;
}

static uint32_t piece_of(uint32_t off)
{
    uint32_t k = (off - ln.start) / PIECE;

    k = k < ln.pieces ? k : ln.pieces - 1u;

    return k && off < piece_from(k) ? k - 1u : k;
}

static void bounds(uint32_t row)
{
    ln.start = row_start(row);
    ln.end = row + 1u < hl_rows(ed_now->doc) ? row_start(row + 1u) - 1u : hl_len(ed_now->doc);
    ln.pieces = ln.end > ln.start ? (ln.end - ln.start + PIECE - 1u) / PIECE : 1u;
}

static void lay(uint32_t row, uint32_t piece)
{
    const uint16_t *run;
    uint32_t sep = ed_now->ext == ED_NONE ? 0u : ed_exts[ed_now->ext].columns;
    uint32_t tints = ed_column_count < COLUMNS ? ed_column_count : COLUMNS;
    uint32_t col = 0u;
    uint32_t off;
    uint32_t got;
    uint32_t stop;
    uint32_t i;
    uint8_t ink;
    int quoted = 0;
    int first;
    int faint;

    sep = tints ? sep : 0u;
    bounds(row);
    ln.piece = piece < ln.pieces ? piece : ln.pieces - 1u;
    ln.from = piece_from(ln.piece);
    ln.to = piece_from(ln.piece + 1u);
    ln.n = 0u;

    for (off = ln.start; sep && off < ln.from; off += got)
    {
        run = hl_text(ed_now->doc, off, &got);
        got = got > ln.from - off ? ln.from - off : got;

        for (i = 0u; i < got; i++)
        {
            quoted ^= run[i] == '"';
            col += run[i] == sep && !quoted ? 1u : 0u;
        }
    }

    off = ln.from;

    while (off < ln.to && ln.n + TAB < CELLS)
    {
        run = hl_text(ed_now->doc, off, &got);
        got = got > ln.to - off ? ln.to - off : got;

        for (i = 0u; i < got && ln.n + TAB < CELLS; i++)
        {
            stop = run[i] == '\t' ? TAB - ln.n % TAB : 1u;
            quoted ^= run[i] == '"';
            ink = (uint8_t)(sep ? COLUMN + col % tints : PLAIN);

            if (sep && run[i] == sep && !quoted)
            {
                ink = FAINT;
                col++;
            }

            for (first = 1; stop; stop--, first = 0)
            {
                ln.cells[ln.n] = picture(run[i], first, &faint);
                ln.ink[ln.n] = faint ? FAINT : ink;
                ln.off[ln.n++] = off + i;
            }
        }

        off += i;
    }

    ln.off[ln.n] = off;
    place();
    wrap();
}

static void lay_at(uint32_t off)
{
    uint32_t row = row_of(off);

    bounds(row);
    lay(row, piece_of(off));
}

static uint32_t cell_of(uint32_t off)
{
    uint32_t lo = 0u;
    uint32_t hi = ln.n;
    uint32_t mid;

    while (lo < hi)
    {
        mid = (lo + hi) / 2u;

        if (ln.off[mid] < off)
        {
            lo = mid + 1u;
        }

        else
        {
            hi = mid;
        }
    }

    return lo;
}

static uint32_t sub_of(uint32_t c)
{
    uint32_t lo = 0u;
    uint32_t hi = ln.subs - 1u;
    uint32_t mid;

    while (lo < hi)
    {
        mid = (lo + hi + 1u) / 2u;

        if (ln.brk[mid] <= c)
        {
            lo = mid;
        }

        else
        {
            hi = mid - 1u;
        }
    }

    return lo;
}

static void colour(void)
{
    static uint32_t spans[SPANS * HL_SPAN_WORDS];
    uint32_t from = ln.from;
    uint32_t got;
    uint32_t i;
    uint32_t c;
    uint32_t to;

    do
    {
        got = hl_spans(ed_now->doc, from, ln.off[ln.n], spans, SPANS);

        for (i = 0u; i < got; i++)
        {
            from = spans[i * HL_SPAN_WORDS + HL_SPAN_AT] + spans[i * HL_SPAN_WORDS + HL_SPAN_LEN];
            to = cell_of(from);

            for (c = cell_of(spans[i * HL_SPAN_WORDS + HL_SPAN_AT]); c < to; c++)
            {
                if (ln.ink[c] != FAINT)
                {
                    ln.ink[c] = (uint8_t)spans[i * HL_SPAN_WORDS + HL_SPAN_ID];
                }
            }
        }
    } while (got == SPANS);
}

static int walk(uint32_t *row, uint32_t *sub, int32_t by)
{
    uint32_t last = hl_rows(ed_now->doc) - 1u;
    uint32_t s = *sub % SUBS;
    int moved = 1;

    lay(*row, *sub / SUBS);
    s = s < ln.subs ? s : ln.subs - 1u;

    for (; by < 0 && moved; by++)
    {
        if (s)
        {
            s--;
        }

        else if (ln.piece)
        {
            lay(*row, ln.piece - 1u);
            s = ln.subs - 1u;
        }

        else if (*row)
        {
            *row -= 1u;
            lay(*row, LAST);
            s = ln.subs - 1u;
        }

        else
        {
            moved = 0;
        }
    }

    for (; by > 0 && moved; by--)
    {
        if (s + 1u < ln.subs)
        {
            s++;
        }

        else if (ln.piece + 1u < ln.pieces)
        {
            lay(*row, ln.piece + 1u);
            s = 0u;
        }

        else if (*row < last)
        {
            *row += 1u;
            lay(*row, 0u);
            s = 0u;
        }

        else
        {
            moved = 0;
        }
    }

    *sub = ln.piece * SUBS + s;

    return moved;
}

static uint32_t sub_at(uint32_t off)
{
    lay_at(off);

    return ln.piece * SUBS + sub_of(cell_of(off));
}

static int folds(void)
{
    return ed_opt.wrap || hl_len(ed_now->doc) / hl_rows(ed_now->doc) > 512u;
}

static uint32_t off_in(uint32_t sub, int32_t x)
{
    const uint16_t *run;
    uint32_t from = ln.brk[sub];
    uint32_t to = ln.brk[sub + 1u];
    uint32_t lo = fit(from, x < 0 ? 0 : x);
    uint32_t first;
    uint32_t last;
    uint32_t off;
    uint32_t n;

    lo = lo > to ? to : lo;
    lo -= lo > from && lo < ln.n && is_low(ln.cells[lo]) && is_high(ln.cells[lo - 1u]) ? 1u : 0u;
    n = lo + 1u < to && is_high(ln.cells[lo]) && is_low(ln.cells[lo + 1u]) ? 2u : 1u;

    if (lo < to && x - (ln.x[lo] - ln.x[from]) > ln.x[lo + n] - ln.x[from] - x)
    {
        lo += n;
    }

    if (lo == to && (sub + 1u < ln.subs || ln.piece + 1u < ln.pieces) && lo > from)
    {
        lo--;
    }

    off = ln.off[lo];

    for (first = lo; first && ln.off[first - 1u] == off; first--)
    {
    }

    for (last = lo; last < ln.n && ln.off[last] == off; last++)
    {
    }

    if (lo > first && (lo - first) * 2u >= last - first)
    {
        off = ln.off[last];
    }

    run = hl_text(ed_now->doc, off, &n);

    if (off > ln.start && n && is_low(run[0]))
    {
        off--;
    }

    return off;
}

int32_t ed_x_of(uint32_t off)
{
    uint32_t c;

    ed_measure();
    lay_at(off);
    c = cell_of(off);

    return ln.x[c] - ln.x[ln.brk[sub_of(c)]];
}

uint32_t ed_col_of(uint32_t off)
{
    ed_measure();
    lay_at(off);

    return ln.from - ln.start + cell_of(off);
}

uint32_t ed_off_lines(uint32_t off, int32_t by, int32_t x)
{
    uint32_t row = row_of(off);
    uint32_t sub;

    ed_measure();
    sub = sub_at(off);

    if (!walk(&row, &sub, by))
    {
        return by < 0 ? 0u : hl_len(ed_now->doc);
    }

    return off_in(sub % SUBS, x);
}

uint32_t ed_line_start(uint32_t off)
{
    ed_measure();
    lay_at(off);

    return ln.off[ln.brk[sub_of(cell_of(off))]];
}

uint32_t ed_line_end(uint32_t off)
{
    uint32_t sub;

    ed_measure();
    lay_at(off);
    sub = sub_of(cell_of(off));

    if (sub + 1u < ln.subs)
    {
        return ln.off[ln.brk[sub + 1u] - 1u];
    }

    return ln.piece + 1u < ln.pieces && ln.n ? ln.off[ln.n - 1u] : ln.end;
}

uint32_t ed_off_at(int32_t x, int32_t y)
{
    uint32_t row = ed_now->top;
    uint32_t sub = ed_now->sub;

    ed_measure();
    walk(&row, &sub, (y - ed_g.text_y) / ed_g.lh - (y < ed_g.text_y ? 1 : 0));

    return off_in(sub % SUBS, x - ed_g.text_x + ed_now->left);
}

void ed_scroll_lines(int32_t by)
{
    ed_measure();
    walk(&ed_now->top, &ed_now->sub, by);
}

void ed_reveal(void)
{
    struct ed_tab *t = ed_now;
    uint32_t shown = ed_rows_shown();
    uint32_t row = row_of(t->caret);
    uint32_t sub = sub_at(t->caret);
    uint32_t c = cell_of(t->caret);
    int32_t x = ln.x[c] - ln.x[ln.brk[sub % SUBS]];

    if (row < t->top || (row == t->top && sub < t->sub))
    {
        t->top = row;
        t->sub = sub;
    }

    walk(&row, &sub, 1 - (int32_t)(shown ? shown : 1u));

    if (row > t->top || (row == t->top && sub > t->sub))
    {
        t->top = row;
        t->sub = sub;
    }

    if (ed_opt.wrap)
    {
        t->left = 0;
        return;
    }

    if (x < t->left + ED_PAD)
    {
        t->left = x > ed_g.text_w / 4 ? x - ed_g.text_w / 4 : 0;
    }

    if (x > t->left + ed_g.text_w - ED_PAD * 2)
    {
        t->left = x - ed_g.text_w + ed_g.text_w / 4;
    }
}

void ed_scroll_to(int32_t y)
{
    uint32_t last;
    uint32_t off;

    ed_measure();
    last = hl_rows(ed_now->doc) - 1u;
    y = y < ed_g.text_y ? ed_g.text_y : y;
    y = y > ed_g.text_y + ed_g.text_h ? ed_g.text_y + ed_g.text_h : y;

    if (!folds())
    {
        ed_now->top = (uint32_t)((uint64_t)(uint32_t)(y - ed_g.text_y) * last / (uint32_t)ed_g.text_h);
        ed_now->sub = 0u;
        return;
    }

    off = (uint32_t)((uint64_t)(uint32_t)(y - ed_g.text_y) * hl_len(ed_now->doc) / (uint32_t)ed_g.text_h);
    ed_now->top = row_of(off);
    ed_now->sub = sub_at(off);
}

static int32_t tab_width(uint32_t pos, uint16_t *label, uint32_t *n)
{
    uint32_t tab = sess_at(pos);

    *n = ed_tab_name(tab, label, NAME);

    if (sess_unsaved(tab))
    {
        label[(*n)++] = ' ';
        label[(*n)++] = '*';
    }

    return ED_PAD + ed_text_width(label, *n) + ED_PAD + ed_say_width("x") + ED_PAD;
}

uint32_t ed_hit(int32_t x, int32_t y, uint32_t *pos)
{
    uint16_t label[NAME + 2u];
    uint32_t n;
    int32_t left = -bar_left;
    int32_t w;

    ed_measure();

    if (y < ed_g.bar_y || y >= ed_g.panel_y)
    {
        return ED_HIT_NONE;
    }

    if (y >= ed_g.text_y)
    {
        return x >= ed_w - ed_g.scroll_w ? ED_HIT_SCROLL : ED_HIT_TEXT;
    }

    ed_text_font(ED_FONT_UI, 100);

    for (*pos = 0u; *pos < sess_count(); (*pos)++)
    {
        w = tab_width(*pos, label, &n);

        if (x >= left && x < left + w)
        {
            return x >= left + w - ed_say_width("x") - ED_PAD * 2 ? ED_HIT_TAB_CLOSE : ED_HIT_TAB;
        }

        left += w + 1;
    }

    return x >= left && x < left + ed_say_width("+") + ED_PAD * 2 ? ED_HIT_TAB_NEW : ED_HIT_NONE;
}

static void draw_bar(void)
{
    uint16_t label[NAME + 2u];
    uint32_t pos;
    uint32_t n;
    int32_t left = 0;
    int32_t y = ed_g.bar_y;
    int32_t w;
    int active;

    ed_text_font(ED_FONT_UI, 100);

    for (pos = 0u; pos < sess_active(); pos++)
    {
        left += tab_width(pos, label, &n) + 1;
    }

    w = tab_width(sess_active(), label, &n);

    w += sess_active() + 1u == sess_count() ? 1 + ed_say_width("+") + ED_PAD * 2 : 0;
    bar_left = left < bar_left ? left : bar_left;
    bar_left = left + w > bar_left + ed_w ? left + w - ed_w : bar_left;
    ed_draw_clip(0, y, ed_w, ed_g.bar_h);
    ed_draw_rect(0, y, ed_w, ed_g.bar_h, ed_colours[ED_C_BAR_BACK]);
    left = -bar_left;

    for (pos = 0u; pos < sess_count(); pos++)
    {
        active = pos == sess_active();
        w = tab_width(pos, label, &n);
        ed_draw_rect(left, y + (active ? 0 : ed_px(2)), w, ed_g.bar_h, ed_colours[active ? ED_C_TAB_ACTIVE_BACK : ED_C_TAB_BACK]);

        if (active)
        {
            ed_draw_rect(left, y, w, ed_px(2), ed_colours[ED_C_ACCENT]);
        }

        ed_draw_text(left + ED_PAD, y + ED_PAD, label, n, ed_colours[active ? ED_C_TAB_ACTIVE_TEXT : ED_C_TAB_TEXT]);
        ed_say(left + w - ED_PAD - ed_say_width("x"), y + ED_PAD, "x", ed_colours[ED_C_TAB_TEXT]);
        left += w + 1;
    }

    ed_say(left + ED_PAD, y + ED_PAD, "+", ed_colours[ED_C_TAB_TEXT]);
}

static uint32_t digits(uint32_t v, uint16_t *out)
{
    uint16_t back[10];
    uint32_t n = 0u;
    uint32_t i;

    do
    {
        back[n++] = (uint16_t)('0' + v % 10u);
        v /= 10u;
    } while (v);

    for (i = 0u; i < n; i++)
    {
        out[i] = back[n - 1u - i];
    }

    return n;
}

static uint32_t ink_rgb(uint8_t ink)
{
    if (ink == PLAIN)
    {
        return ed_colours[ED_C_TEXT];
    }

    if (ink >= COLUMN && ink < COLUMN + COLUMNS)
    {
        return ed_columns[ink - COLUMN];
    }

    return ink == FAINT ? ed_colours[ED_C_DIM] : ed_syntax[ink];
}

static void draw_sub(uint32_t sub, int32_t y, uint32_t from, uint32_t to)
{
    uint32_t first = ln.brk[sub];
    uint32_t stop = ln.brk[sub + 1u];
    uint32_t lo = ln.off[first];
    uint32_t hi = ln.off[stop];
    uint32_t c = first;
    uint32_t run;
    int32_t x0 = ed_g.text_x - ed_now->left - ln.x[first];
    int32_t a;
    int32_t b;
    int closes = sub + 1u == ln.subs && ln.piece + 1u == ln.pieces;

    if (from < to && to > lo && from <= hi)
    {
        a = ln.x[cell_of(from > lo ? from : lo)];
        b = ln.x[cell_of(to < hi ? to : hi)];
        b += to > ln.end && closes ? ed_say_width(" ") : 0;

        if (b > a)
        {
            ed_draw_rect(x0 + a, y, b - a, ed_g.lh, ed_colours[ED_C_SELECTION]);
        }
    }

    while (c < stop)
    {
        for (run = c; run < stop && ln.ink[run] == ln.ink[c]; run++)
        {
        }

        if (x0 + ln.x[c] > ed_w)
        {
            return;
        }

        if (x0 + ln.x[run] >= 0)
        {
            ed_draw_text(x0 + ln.x[c], y, ln.cells + c, run - c, ink_rgb(ln.ink[c]));
        }

        c = run;
    }
}

static void draw_caret(uint32_t c, int32_t y)
{
    int32_t x = ed_g.text_x - ed_now->left + ln.x[c] - ln.x[ln.brk[sub_of(c)]];

    if (!ed_opt.overwrite)
    {
        ed_draw_rect(x, y, ed_px(2), ed_g.lh, ed_colours[ED_C_CARET]);
        return;
    }

    ed_draw_rect(x, y + ed_g.lh - ed_px(2), c < ln.n ? ed_text_width(ln.cells + c, 1u) : ed_say_width(" "), ed_px(2), ed_colours[ED_C_CARET]);
}

static void draw_piece(uint32_t row, uint32_t sub, int32_t *y, int lit)
{
    struct ed_tab *t = ed_now;
    uint16_t number[10];
    uint32_t from = ed_sel_from();
    uint32_t to = ed_sel_to();
    uint32_t cell = cell_of(t->caret);
    uint32_t n;
    int32_t bottom = ed_g.text_y + ed_g.text_h;
    int here = lit && t->caret >= ln.from && (t->caret < ln.to || ln.piece + 1u == ln.pieces);

    colour();

    if (!ln.piece && !sub)
    {
        ed_draw_clip(0, ed_g.text_y, ed_g.gutter_w, ed_g.text_h);
        n = digits(row + 1u, number);
        ed_draw_text(ed_g.gutter_w - ED_PAD - ed_text_width(number, n), *y, number, n, ed_colours[ED_C_GUTTER_TEXT]);
    }

    ed_draw_clip(ed_g.text_x, ed_g.text_y, ed_g.text_w, ed_g.text_h);

    for (; sub < ln.subs && *y < bottom; sub++, *y += ed_g.lh)
    {
        if (lit && from == to)
        {
            ed_draw_rect(ed_g.text_x, *y, ed_g.text_w, ed_g.lh, ed_colours[ED_C_LINE]);
        }

        draw_sub(sub, *y, from, to);
        seen_to = ln.off[ln.brk[sub + 1u]] + (sub + 1u == ln.subs && ln.piece + 1u == ln.pieces ? 1u : 0u);

        if (here && sub == sub_of(cell))
        {
            draw_caret(cell, *y);
        }
    }
}

static void draw_text(void)
{
    struct ed_tab *t = ed_now;
    uint32_t rows = hl_rows(t->doc);
    uint32_t caret_row = row_of(t->caret);
    uint32_t piece = t->sub / SUBS;
    uint32_t sub = t->sub % SUBS;
    uint32_t row;
    int32_t bottom = ed_g.text_y + ed_g.text_h;
    int32_t y = ed_g.text_y;

    ed_draw_clip(0, ed_g.text_y, ed_w, ed_g.text_h);
    ed_draw_rect(0, ed_g.text_y, ed_w, ed_g.text_h, ed_colours[ED_C_BACK]);
    ed_draw_rect(0, ed_g.text_y, ed_g.gutter_w, ed_g.text_h, ed_colours[ED_C_GUTTER_BACK]);
    lay(t->top, piece);
    sub = sub < ln.subs ? sub : 0u;
    seen_from = ln.off[ln.brk[sub]];
    seen_to = seen_from;

    for (row = t->top; row < rows && y < bottom; row++)
    {
        for (; y < bottom; piece++)
        {
            lay(row, piece);
            draw_piece(row, sub, &y, row == caret_row);
            sub = 0u;

            if (ln.piece + 1u >= ln.pieces)
            {
                break;
            }
        }

        piece = 0u;
    }
}

static void draw_scroll(void)
{
    uint32_t rows = hl_rows(ed_now->doc);
    uint32_t shown = (uint32_t)(ed_g.text_h / ed_g.lh);
    uint32_t all = hl_len(ed_now->doc) + 1u;
    uint32_t span = seen_to - seen_from < all ? seen_to - seen_from : all;
    int32_t h = (int32_t)((uint64_t)(uint32_t)ed_g.text_h * shown / (rows + shown - 1u));
    int32_t w = ed_g.scroll_w;
    int32_t y;

    h = folds() ? (int32_t)((uint64_t)(uint32_t)ed_g.text_h * span / all) : h;
    h = h < ed_g.lh ? ed_g.lh : h;
    h = h > ed_g.text_h ? ed_g.text_h : h;
    y = rows > 1u ? (int32_t)((uint64_t)(uint32_t)(ed_g.text_h - h) * ed_now->top / (rows - 1u)) : 0;

    if (folds())
    {
        y = all > span ? (int32_t)((uint64_t)(uint32_t)(ed_g.text_h - h) * seen_from / (all - span)) : 0;
    }

    ed_draw_clip(ed_w - w, ed_g.text_y, w, ed_g.text_h);
    ed_draw_rect(ed_w - w, ed_g.text_y, w, ed_g.text_h, ed_colours[ED_C_GUTTER_BACK]);
    ed_draw_rect(ed_w - w + ed_px(2), ed_g.text_y + y, w - ed_px(4), h, ed_colours[ED_C_SCROLL]);
}

void ed_view_draw(void)
{
    ed_measure();

    if (ed_now && ed_now->loaded)
    {
        draw_text();
        draw_scroll();
    }

    draw_bar();
}
