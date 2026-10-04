#include "ed_int.h"

#define ITEMS 48u
#define LABEL 48u
#define TABS 30u
#define TITLE_PAD ed_px(10)
#define TICK_W ed_px(22)
#define KEY_GAP ed_px(28)
#define MENUS 7u

static const char mnemonics[MENUS + 1u] = "fesvnlw";

struct item {
    uint16_t label[LABEL];
    uint32_t n;
    uint32_t cmd, arg;
};

static struct item items[ITEMS];

static uint32_t item_n;
static int32_t at_x[ITEMS];
static int32_t at_y[ITEMS];
static int32_t col_w;
static uint32_t open = ED_NONE;
static uint32_t hot = ED_NONE;
static uint32_t hover = ED_NONE;

static struct item *add(const char *label, uint32_t cmd, uint32_t arg)
{
    struct item *it;
    uint32_t n = 0u;
    item_n -= item_n == ITEMS ? 1u : 0u;
    it = &items[item_n];

    while (label[n])
    {
        n++;
    }

    it->n = ed_utf8_units(label, n, it->label, LABEL);
    it->cmd = cmd;
    it->arg = arg;
    item_n++;

    return it;
}

static void rule(void)
{
    add("", 0u, 0u);
}

static void add_languages(void)
{
    uint32_t i;
    uint32_t j;
    uint32_t k;

    for (i = 0u; i < ed_ext_count; i++)
    {
        for (j = 0u; j < i; j++)
        {
            for (k = 0u; ed_exts[i].tag[k] && ed_exts[i].tag[k] == ed_exts[j].tag[k]; k++)
            {
            }

            if (ed_exts[i].tag[k] == ed_exts[j].tag[k])
            {
                break;
            }
        }

        if (j == i)
        {
            add(ed_exts[i].title, ED_CMD_LANGUAGE, i);
        }
    }
}

static void add_tabs(void)
{
    struct item *it;
    uint32_t pos;

    for (pos = 0u; pos < sess_count() && pos < TABS; pos++)
    {
        it = add("", ED_CMD_TAB, pos);
        it->n = ed_tab_name(sess_at(pos), it->label, LABEL);
    }
}

static void build(uint32_t menu)
{
    item_n = 0u;

    switch (menu)
    {
        case 0u:
            add(ed_word(ED_W_NEW), ED_CMD_NEW, 0u);
            add(ed_word(ED_W_OPEN), ED_CMD_OPEN, 0u);
            add(ed_word(ED_W_RELOAD), ED_CMD_RELOAD, 0u);
            rule();
            add(ed_word(ED_W_SAVE), ED_CMD_SAVE, 0u);
            add(ed_word(ED_W_SAVE_AS), ED_CMD_SAVE_AS, 0u);
            add(ed_word(ED_W_SAVE_ALL), ED_CMD_SAVE_ALL, 0u);
            rule();
            add(ed_word(ED_W_CLOSE), ED_CMD_CLOSE, 0u);
            add(ed_word(ED_W_CLOSE_ALL), ED_CMD_CLOSE_ALL, 0u);
            add(ed_word(ED_W_CLOSE_OTHERS), ED_CMD_CLOSE_OTHERS, 0u);
            rule();

            if (ed_erase_offered)
            {
                add(ed_word(ED_W_ERASE), ED_CMD_ERASE, 0u);
                rule();
            }

            add(ed_word(ED_W_EXIT), ED_CMD_EXIT, 0u);
            break;

        case 1u:
            add(ed_word(ED_W_UNDO), ED_CMD_UNDO, 0u);
            add(ed_word(ED_W_REDO), ED_CMD_REDO, 0u);
            rule();
            add(ed_word(ED_W_CUT), ED_CMD_CUT, 0u);
            add(ed_word(ED_W_COPY), ED_CMD_COPY, 0u);
            add(ed_word(ED_W_PASTE), ED_CMD_PASTE, 0u);
            add(ed_word(ED_W_DELETE), ED_CMD_DELETE, 0u);
            add(ed_word(ED_W_SELECT_ALL), ED_CMD_SELECT_ALL, 0u);
            rule();
            add(ed_word(ED_W_DUP_LINE), ED_CMD_DUP_LINE, 0u);
            add(ed_word(ED_W_DEL_LINE), ED_CMD_DEL_LINE, 0u);
            add(ed_word(ED_W_LINE_UP), ED_CMD_LINE_UP, 0u);
            add(ed_word(ED_W_LINE_DOWN), ED_CMD_LINE_DOWN, 0u);
            rule();
            add(ed_word(ED_W_INDENT), ED_CMD_INDENT, 0u);
            add(ed_word(ED_W_UNINDENT), ED_CMD_UNINDENT, 0u);
            add(ed_word(ED_W_UPPER), ED_CMD_UPPER, 0u);
            add(ed_word(ED_W_LOWER), ED_CMD_LOWER, 0u);
            add(ed_word(ED_W_TRIM), ED_CMD_TRIM, 0u);
            rule();
            add(ed_word(ED_W_EOL_CRLF), ED_CMD_EOL, ED_EOL_CRLF);
            add(ed_word(ED_W_EOL_LF), ED_CMD_EOL, ED_EOL_LF);
            add(ed_word(ED_W_EOL_CR), ED_CMD_EOL, ED_EOL_CR);
            rule();
            add(ed_word(ED_W_OVERWRITE), ED_CMD_OVERWRITE, 0u);
            break;

        case 2u:
            add(ed_word(ED_W_FIND), ED_CMD_FIND, 0u);
            add(ed_word(ED_W_FIND_NEXT), ED_CMD_FIND_NEXT, 0u);
            add(ed_word(ED_W_FIND_PREV), ED_CMD_FIND_PREV, 0u);
            add(ed_word(ED_W_REPLACE), ED_CMD_REPLACE, 0u);
            rule();
            add(ed_word(ED_W_GOTO), ED_CMD_GOTO, 0u);
            break;

        case 3u:
            add(ed_word(ED_W_TOOLBAR), ED_CMD_TOOLBAR, 0u);
            add(ed_word(ED_W_STATUS), ED_CMD_STATUS, 0u);
            add(ed_word(ED_W_NUMBERS), ED_CMD_NUMBERS, 0u);
            add(ed_word(ED_W_SPACES), ED_CMD_SPACES, 0u);
            add(ed_word(ED_W_WRAP), ED_CMD_WRAP, 0u);
            rule();
            add(ed_word(ED_W_ZOOM_IN), ED_CMD_ZOOM_IN, 0u);
            add(ed_word(ED_W_ZOOM_OUT), ED_CMD_ZOOM_OUT, 0u);
            add(ed_word(ED_W_ZOOM_RESET), ED_CMD_ZOOM_RESET, 0u);
            rule();
            add(ed_word(ED_W_FONT_TEXT), ED_CMD_FONT_TEXT, 0u);
            add(ed_word(ED_W_FONT_UI), ED_CMD_FONT_UI, 0u);
            add(ed_word(ED_W_TONGUE), ED_CMD_TONGUE, 0u);
            break;

        case 4u:
            add("UTF-8", ED_CMD_ENCODING, ED_ENC_UTF8);
            add("UTF-8-BOM", ED_CMD_ENCODING, ED_ENC_UTF8_BOM);
            add("UTF-16 LE BOM", ED_CMD_ENCODING, ED_ENC_UTF16_LE);
            add("UTF-16 BE BOM", ED_CMD_ENCODING, ED_ENC_UTF16_BE);
            add("ANSI", ED_CMD_ENCODING, ED_ENC_LATIN1);
            break;

        case 5u:
            add(ed_word(ED_W_NORMAL_TEXT), ED_CMD_LANGUAGE, ED_NONE);
            add_languages();
            break;

        default:
            add(ed_word(ED_W_TAB_NEXT), ED_CMD_TAB_NEXT, 0u);
            add(ed_word(ED_W_TAB_PREV), ED_CMD_TAB_PREV, 0u);
            rule();
            add_tabs();
            break;
    }
}

static void show(uint32_t menu)
{
    open = menu;
    hot = ED_NONE;

    if (menu != ED_NONE)
    {
        build(menu);
    }

    ed_touch();
}

static int32_t title_width(uint32_t menu)
{
    return ed_say_width(ed_word(ED_W_MENU_FILE + menu)) + TITLE_PAD * 2;
}

static int32_t title_x(uint32_t menu)
{
    int32_t x = ed_px(2);
    uint32_t i;

    for (i = 0u; i < menu; i++)
    {
        x += title_width(i);
    }

    return x;
}

static uint32_t title_at(int32_t x, int32_t y)
{
    uint32_t i;

    for (i = 0u; i < MENUS && y >= 0 && y < ed_g.menu_h; i++)
    {
        if (x >= title_x(i) && x < title_x(i) + title_width(i))
        {
            return i;
        }
    }

    return ED_NONE;
}

static int32_t item_height(const struct item *it)
{
    return it->cmd ? ed_g.ui_lh + ed_px(6) : ed_px(7);
}

struct box {
    int32_t x, y, w, h;
};

static struct box drop(void)
{
    struct box b;
    const char *key;
    int32_t room = ed_h - ed_g.menu_h - ed_px(2);
    int32_t edge = ed_px(2);
    int32_t w;
    int32_t x = 0;
    int32_t y = edge;
    uint32_t i;

    col_w = 0;
    b.h = 0;

    for (i = 0u; i < item_n; i++)
    {
        key = ed_cmd_key(items[i].cmd, items[i].arg);
        w = TICK_W + ed_text_width(items[i].label, items[i].n) + (key ? KEY_GAP + ed_say_width(key) : 0) + ED_PAD * 2;
        col_w = w > col_w ? w : col_w;
    }

    for (i = 0u; i < item_n; i++)
    {
        if (y > edge && y + item_height(&items[i]) > room)
        {
            x += col_w;
            y = edge;
        }

        at_x[i] = x;
        at_y[i] = y;
        y += item_height(&items[i]);
        b.h = y + edge > b.h ? y + edge : b.h;
    }

    b.w = x + col_w;
    b.x = title_x(open);
    b.x = b.x + b.w > ed_w ? ed_w - b.w : b.x;
    b.x = b.x < 0 ? 0 : b.x;
    b.y = ed_g.menu_h;

    return b;
}

static uint32_t item_at(int32_t x, int32_t y, int *inside)
{
    struct box b = drop();
    uint32_t i;

    *inside = x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h;

    for (i = 0u; *inside && i < item_n; i++)
    {
        if (x >= b.x + at_x[i] && x < b.x + at_x[i] + col_w && y >= b.y + at_y[i] && y < b.y + at_y[i] + item_height(&items[i]))
        {
            return items[i].cmd ? i : ED_NONE;
        }
    }

    return ED_NONE;
}

static uint32_t step(uint32_t from, int back)
{
    uint32_t i = from;
    uint32_t tries;

    for (tries = 0u; tries < item_n; tries++)
    {
        if (i == ED_NONE)
        {
            i = back ? item_n - 1u : 0u;
        }

        else
        {
            i = (i + (back ? item_n - 1u : 1u)) % item_n;
        }

        if (items[i].cmd && ed_can(items[i].cmd, items[i].arg))
        {
            return i;
        }
    }

    return from;
}

static void choose(uint32_t i)
{
    uint32_t cmd = items[i].cmd;
    uint32_t arg = items[i].arg;

    if (!ed_can(cmd, arg))
    {
        return;
    }

    show(ED_NONE);
    ed_run(cmd, arg);
}

int ed_menu_open(void)
{
    return open != ED_NONE;
}

int ed_menu_down(int32_t x, int32_t y)
{
    uint32_t title;
    uint32_t i;
    int inside;

    ed_text_font(ED_FONT_UI, 100);
    title = title_at(x, y);

    if (title != ED_NONE)
    {
        show(title == open ? ED_NONE : title);

        return 1;
    }

    if (open == ED_NONE)
    {
        return y < ed_g.menu_h;
    }

    i = item_at(x, y, &inside);

    if (i != ED_NONE)
    {
        choose(i);
    }

    else if (!inside)
    {
        show(ED_NONE);
    }

    return 1;
}

int ed_menu_move(int32_t x, int32_t y)
{
    uint32_t title;
    uint32_t was_hot = hot;
    uint32_t was_hover = hover;
    int inside;

    ed_text_font(ED_FONT_UI, 100);
    title = title_at(x, y);
    hover = title;

    if (open != ED_NONE && title != ED_NONE && title != open)
    {
        show(title);
    }

    else if (open != ED_NONE)
    {
        hot = item_at(x, y, &inside);
    }

    if (hot != was_hot || hover != was_hover)
    {
        ed_touch();
    }

    return open != ED_NONE;
}

int ed_menu_key(uint32_t key, uint32_t mods)
{
    uint32_t i;

    for (i = 0u; i < MENUS && mods == ED_MOD_ALT; i++)
    {
        if (key == (uint8_t)mnemonics[i])
        {
            show(i);
            hot = step(ED_NONE, 0);

            return 1;
        }
    }

    if (open == ED_NONE)
    {
        return 0;
    }

    switch (key)
    {
        case ED_KEY_ESCAPE:
            show(ED_NONE);
            break;

        case ED_KEY_UP:
            hot = step(hot, 1);
            break;

        case ED_KEY_DOWN:
            hot = step(hot, 0);
            break;

        case ED_KEY_LEFT:
            show((open + MENUS - 1u) % MENUS);
            hot = step(ED_NONE, 0);
            break;

        case ED_KEY_RIGHT:
            show((open + 1u) % MENUS);
            hot = step(ED_NONE, 0);
            break;

        case ED_KEY_ENTER:
            if (hot != ED_NONE)
            {
                choose(hot);
            }

            break;

        default:
            break;
    }

    ed_touch();

    return 1;
}

void ed_menu_draw(void)
{
    uint32_t i;
    int32_t x;

    ed_text_font(ED_FONT_UI, 100);
    ed_draw_clip(0, 0, ed_w, ed_g.menu_h);
    ed_draw_rect(0, 0, ed_w, ed_g.menu_h, ed_colours[ED_C_MENU_BACK]);

    for (i = 0u; i < MENUS; i++)
    {
        x = title_x(i);

        if (i == open || i == hover)
        {
            ed_draw_rect(x, ed_px(2), title_width(i), ed_g.menu_h - ed_px(4), ed_colours[ED_C_MENU_HOT]);
        }

        ed_say(x + TITLE_PAD, ed_px(4), ed_word(ED_W_MENU_FILE + i), ed_colours[ED_C_MENU_TEXT]);
    }
}

void ed_menu_draw_open(void)
{
    struct box b;
    const char *key;
    uint32_t i;
    uint32_t ink;
    int32_t x;
    int32_t y;
    int32_t h;

    if (open == ED_NONE)
    {
        return;
    }

    ed_text_font(ED_FONT_UI, 100);
    b = drop();
    ed_draw_clip(0, 0, ed_w, ed_h);
    ed_draw_rect(b.x, b.y, b.w, b.h, ed_colours[ED_C_BORDER]);
    ed_draw_rect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, ed_colours[ED_C_MENU_BACK]);

    for (i = 0u; i < item_n; i++)
    {
        x = b.x + at_x[i];
        y = b.y + at_y[i];
        h = item_height(&items[i]);

        if (at_x[i] && at_y[i] == ed_px(2))
        {
            ed_draw_rect(x, b.y + 1, 1, b.h - 2, ed_colours[ED_C_BORDER]);
        }

        if (!items[i].cmd)
        {
            ed_draw_rect(x + ED_PAD, y + ed_px(3), col_w - ED_PAD * 2, 1, ed_colours[ED_C_BORDER]);
            continue;
        }

        ink = ed_colours[ed_can(items[i].cmd, items[i].arg) ? ED_C_MENU_TEXT : ED_C_DIM];

        if (i == hot)
        {
            ed_draw_rect(x + 1, y, col_w - 2, h, ed_colours[ED_C_MENU_HOT]);
        }

        if (ed_on(items[i].cmd, items[i].arg))
        {
            ed_draw_rect(x + ed_px(8), y + h / 2 - ed_px(3), ed_px(6), ed_px(6), ink);
        }

        ed_draw_text(x + TICK_W, y + ed_px(3), items[i].label, items[i].n, ink);
        key = ed_cmd_key(items[i].cmd, items[i].arg);

        if (key)
        {
            ed_say(x + col_w - ED_PAD - ed_say_width(key), y + ed_px(3), key, ed_colours[ED_C_DIM]);
        }
    }
}
