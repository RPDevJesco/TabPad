#include "ed_int.h"

#define FILTER 32u
#define PICKS 2048u
#define LABEL 80u
#define ROWS 12u

static uint32_t what;
static uint16_t filter[FILTER];
static uint32_t filter_n;
static uint32_t picks[PICKS];
static uint32_t pick_n;
static uint32_t hot;
static uint32_t top;

struct box {
    int32_t x, y, w, h;
    int32_t head, row;
    uint32_t rows;
};

static uint32_t count(void)
{
    return (what == ED_CMD_TONGUE ? ed_tongue_count() : ed_font_count()) + 1u;
}

static uint32_t span(const char *s)
{
    uint32_t n = 0u;

    while (s[n])
    {
        n++;
    }

    return n;
}

static uint32_t label(uint32_t i, uint16_t *out)
{
    const char *text;
    uint32_t n = 0u;

    if (!i)
    {
        text = ed_word(what == ED_CMD_TONGUE ? ED_W_P_ENGLISH : ED_W_P_DEFAULT);

        return ed_utf8_units(text, span(text), out, LABEL);
    }

    if (what != ED_CMD_TONGUE)
    {
        text = ed_font_name(i - 1u);

        return ed_utf8_units(text, span(text), out, LABEL);
    }

    text = ed_tongue_text(i - 1u, &n);
    n = ed_word_name(text, n, out, LABEL);
    text = ed_tongue_tag(i - 1u);

    return n ? n : ed_utf8_units(text, span(text), out, LABEL);
}

static const char *chosen(void)
{
    if (what == ED_CMD_TONGUE)
    {
        return ed_opt.tongue;
    }

    return what == ED_CMD_FONT_UI ? ed_opt.font_ui : ed_opt.font_text;
}

static const char *value(uint32_t i)
{
    if (!i)
    {
        return "";
    }

    return what == ED_CMD_TONGUE ? ed_tongue_tag(i - 1u) : ed_font_name(i - 1u);
}

static int same(const char *a, const char *b)
{
    while (*a && *a == *b)
    {
        a++;
        b++;
    }

    return *a == *b;
}

static uint32_t fold(uint32_t u)
{
    return u >= 'A' && u <= 'Z' ? u + 32u : u;
}

static int passes(uint32_t i)
{
    uint16_t text[LABEL];
    uint32_t n = label(i, text);
    uint32_t at;
    uint32_t k;

    for (at = 0u; at + filter_n <= n; at++)
    {
        for (k = 0u; k < filter_n && fold(text[at + k]) == fold(filter[k]); k++)
        {
        }

        if (k == filter_n)
        {
            return 1;
        }
    }

    return 0;
}

static void sift(void)
{
    uint32_t n = count();
    uint32_t i;

    pick_n = 0u;
    hot = 0u;
    top = 0u;

    for (i = 0u; i < n && pick_n < PICKS; i++)
    {
        if (!passes(i))
        {
            continue;
        }

        if (same(value(i), chosen()))
        {
            hot = pick_n;
        }

        picks[pick_n++] = i;
    }
}

static struct box place(void)
{
    struct box b;
    int32_t room;

    ed_text_font(ED_FONT_UI, 100);
    b.row = ed_g.ui_lh + ed_px(6);
    b.head = b.row * 2 + ed_px(10);
    b.w = ed_w - ed_px(40) < ed_px(420) ? ed_w - ed_px(40) : ed_px(420);
    b.w = b.w < ed_px(120) ? ed_px(120) : b.w;
    b.x = (ed_w - b.w) / 2;
    b.y = ed_g.bar_y;
    room = (ed_h - b.y - b.head - ed_px(12)) / b.row;
    b.rows = room < 1 ? 1u : (uint32_t)room;
    b.rows = b.rows > ROWS ? ROWS : b.rows;
    b.h = b.head + (int32_t)b.rows * b.row + ed_px(4);

    return b;
}

static void keep_shown(void)
{
    struct box b = place();

    top = hot < top ? hot : top;
    top = hot >= top + b.rows ? hot - b.rows + 1u : top;
}

static void copy(char *to, const char *from, uint32_t max)
{
    uint32_t i;

    for (i = 0u; i + 1u < max && from[i]; i++)
    {
        to[i] = from[i];
    }

    to[i] = 0;
}

static void choose(void)
{
    const char *v;
    uint32_t kind = what;

    if (hot >= pick_n)
    {
        return;
    }

    v = value(picks[hot]);
    what = 0u;

    if (kind == ED_CMD_TONGUE)
    {
        copy(ed_opt.tongue, v, ED_TAG_BYTES);
        ed_tongue_apply();
    }

    else if (!ed_font_use(kind == ED_CMD_FONT_UI ? ED_FONT_UI : ED_FONT_TEXT, v))
    {
        copy(kind == ED_CMD_FONT_UI ? ed_opt.font_ui : ed_opt.font_text, v, ED_NAME_BYTES);
    }

    if (ed_now && ed_now->loaded)
    {
        ed_reveal();
    }
}

void ed_list_open(uint32_t kind)
{
    what = kind;
    filter_n = 0u;
    sift();
    keep_shown();
}

int ed_list_shown(void)
{
    return what != 0u;
}

void ed_list_key(uint32_t key)
{
    struct box b = place();
    uint32_t last = pick_n ? pick_n - 1u : 0u;

    switch (key)
    {
        case ED_KEY_ESCAPE:
            what = 0u;
            break;

        case ED_KEY_ENTER:
            choose();
            break;

        case ED_KEY_UP:
            hot -= hot ? 1u : 0u;
            break;

        case ED_KEY_DOWN:
            hot += hot < last ? 1u : 0u;
            break;

        case ED_KEY_PAGE_UP:
            hot = hot > b.rows ? hot - b.rows : 0u;
            break;

        case ED_KEY_PAGE_DOWN:
            hot = hot + b.rows < last ? hot + b.rows : last;
            break;

        case ED_KEY_HOME:
            hot = 0u;
            break;

        case ED_KEY_END:
            hot = last;
            break;

        case ED_KEY_BACKSPACE:
            if (filter_n)
            {
                filter_n--;
                sift();
            }

            break;

        default:
            break;
    }

    if (what)
    {
        keep_shown();
    }

    ed_touch();
}

void ed_list_text(const uint16_t *text, uint32_t n)
{
    uint32_t i;

    for (i = 0u; i < n && filter_n < FILTER; i++)
    {
        if (text[i] >= 0x20u)
        {
            filter[filter_n++] = text[i];
        }
    }

    sift();
    ed_touch();
}

static uint32_t row_at(const struct box *b, int32_t x, int32_t y)
{
    int32_t row;

    if (x < b->x || x >= b->x + b->w || y < b->y + b->head || y >= b->y + b->head + (int32_t)b->rows * b->row)
    {
        return ED_NONE;
    }

    row = (y - b->y - b->head) / b->row;

    return top + (uint32_t)row < pick_n ? top + (uint32_t)row : ED_NONE;
}

void ed_list_down(int32_t x, int32_t y)
{
    struct box b = place();
    uint32_t row = row_at(&b, x, y);

    if (row != ED_NONE)
    {
        hot = row;
        choose();
    }

    else if (x < b.x || x >= b.x + b.w || y < b.y || y >= b.y + b.h)
    {
        what = 0u;
    }

    ed_touch();
}

void ed_list_move(int32_t x, int32_t y)
{
    struct box b = place();
    uint32_t row = row_at(&b, x, y);

    if (row != ED_NONE && row != hot)
    {
        hot = row;
        ed_touch();
    }
}

void ed_list_wheel(int32_t rows)
{
    struct box b = place();
    int64_t to = (int64_t)top + rows * 3;
    int64_t most = pick_n > b.rows ? (int64_t)(pick_n - b.rows) : 0;

    to = to < 0 ? 0 : to;
    top = (uint32_t)(to > most ? most : to);
    ed_touch();
}

void ed_list_draw(void)
{
    uint16_t text[LABEL];
    struct box b;
    const char *title;
    uint32_t n;
    uint32_t i;
    int32_t y;
    int32_t pad = ED_PAD;

    if (!what)
    {
        return;
    }

    b = place();
    title = ed_word(what == ED_CMD_TONGUE ? ED_W_P_TONGUE : what == ED_CMD_FONT_UI ? ED_W_P_FONT_UI : ED_W_P_FONT_TEXT);
    ed_draw_clip(0, 0, ed_w, ed_h);
    ed_draw_rect(b.x, b.y, b.w, b.h, ed_colours[ED_C_BORDER]);
    ed_draw_rect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, ed_colours[ED_C_MENU_BACK]);
    ed_draw_clip(b.x + 1, b.y + 1, b.w - 2, b.h - 2);
    ed_say(b.x + pad, b.y + ed_px(4), title, ed_colours[ED_C_MENU_TEXT]);
    y = b.y + b.row + ed_px(2);
    ed_draw_rect(b.x + pad, y, b.w - pad * 2, b.row, ed_colours[ED_C_BORDER]);
    ed_draw_rect(b.x + pad + 1, y + 1, b.w - pad * 2 - 2, b.row - 2, ed_colours[ED_C_FIELD_BACK]);

    if (filter_n)
    {
        ed_draw_text(b.x + pad * 2, y + ed_px(3), filter, filter_n, ed_colours[ED_C_TEXT]);
    }

    else
    {
        ed_say(b.x + pad * 2, y + ed_px(3), ed_word(ED_W_P_FILTER), ed_colours[ED_C_DIM]);
    }

    ed_draw_rect(b.x + pad * 2 + ed_text_width(filter, filter_n), y + ed_px(3), ed_px(2), ed_g.ui_lh, ed_colours[ED_C_CARET]);

    for (i = 0u; i < b.rows && top + i < pick_n; i++)
    {
        y = b.y + b.head + (int32_t)i * b.row;
        n = label(picks[top + i], text);

        if (top + i == hot)
        {
            ed_draw_rect(b.x + 1, y, b.w - 2, b.row, ed_colours[ED_C_MENU_HOT]);
        }

        if (same(value(picks[top + i]), chosen()))
        {
            ed_draw_rect(b.x + pad + ed_px(2), y + b.row / 2 - ed_px(3), ed_px(6), ed_px(6), ed_colours[ED_C_MENU_TEXT]);
        }

        ed_draw_text(b.x + pad + ed_px(16), y + ed_px(3), text, n, ed_colours[ED_C_MENU_TEXT]);
    }
}
