#include "ed_int.h"
#include "port_hosted.h"
#include "store.h"
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

#define FW 8
#define LH 16
#define W 640
#define H 354
#define MENU_H 24
#define TOOL_Y 24
#define BAR_Y 52
#define STATUS_H 24
#define TEXT_X 42
#define TEXT_Y 80
#define ROWS 15

struct rect {
    int32_t x, y, w, h;
    uint32_t rgb;
};

struct glyph {
    int32_t x, y, w, h;
    uint16_t unit;
    uint32_t rgb;
    uint32_t run;
};

static struct rect rects[4096];
static struct glyph glyphs[32768];
static uint32_t rect_n;
static uint32_t glyph_n;
static struct rect clip;
static int32_t fw = FW;
static int32_t lh = LH;
static int32_t size_now = 100;

void ed_draw_clip(int32_t x, int32_t y, int32_t w, int32_t h)
{
    clip.x = x;
    clip.y = y;
    clip.w = w;
    clip.h = h;
}

void ed_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb)
{
    struct rect *r = &rects[rect_n++ % 4096u];
    int32_t x1 = x + w < clip.x + clip.w ? x + w : clip.x + clip.w;
    int32_t y1 = y + h < clip.y + clip.h ? y + h : clip.y + clip.h;

    r->x = x > clip.x ? x : clip.x;
    r->y = y > clip.y ? y : clip.y;
    r->w = x1 - r->x;
    r->h = y1 - r->y;
    r->rgb = rgb;
}

void ed_draw_text(int32_t x, int32_t y, const uint16_t *text, uint32_t n, uint32_t rgb)
{
    struct glyph *g;
    uint32_t i;

    for (i = 0u; i < n; i++, x += fw)
    {
        if (x + fw / 2 < clip.x || x + fw / 2 >= clip.x + clip.w || y + lh / 2 < clip.y || y + lh / 2 >= clip.y + clip.h)
        {
            continue;
        }

        g = &glyphs[glyph_n++ % 32768u];
        g->x = x;
        g->y = y;
        g->w = fw;
        g->h = lh;
        g->unit = text[i];
        g->rgb = rgb;
        g->run = i ? 0u : n;
    }
}

int32_t ed_text_width(const uint16_t *text, uint32_t n)
{
    (void)text;

    return (int32_t)n * fw;
}

int32_t ed_line_height(void)
{
    return lh;
}

static int32_t scale_now = 100;
static uint32_t font_now;
static char font_used[2][ED_NAME_BYTES];
static const char *const fonts[] = { "Alpha Mono", "Beta Sans", "Gamma Serif" };
static const char *const tongues[] = { "xx", "zz" };
static const char *const tongue_texts[] = {
    "\xEF\xBB\xBFname = Test\n# a comment\nfile.new = Neu\r\n  menu.file=Datei  \nstatus.line = Zeile:\nnothing.known = x\nbroken line\nlang.csv = Werte mit Komma\n",
    "file.new = Nouveau\n",
};

void ed_text_font(uint32_t font, int32_t percent)
{
    font_now = font;
    size_now = percent;
    fw = FW * percent / 100 * scale_now / 100;
    lh = LH * percent / 100 * scale_now / 100;
}

uint32_t ed_font_count(void)
{
    return 3u;
}

const char *ed_font_name(uint32_t i)
{
    return fonts[i];
}

int ed_font_use(uint32_t font, const char *name)
{
    uint32_t i;

    for (i = 0u; name[0] && i < 3u && strcmp(name, fonts[i]); i++)
    {
    }

    if (name[0] && i == 3u)
    {
        return -1;
    }

    snprintf(font_used[font], ED_NAME_BYTES, "%s", name);

    return 0;
}

uint32_t ed_tongue_count(void)
{
    return 2u;
}

const char *ed_tongue_tag(uint32_t i)
{
    return tongues[i];
}

const char *ed_tongue_text(uint32_t i, uint32_t *n)
{
    *n = (uint32_t)strlen(tongue_texts[i]);

    return tongue_texts[i];
}

const uint32_t ed_eol_default = ED_EOL_LF;

static const char *base;
static char clipboard[4096];
static uint32_t clipboard_n;
static char title[256];
static int title_unsaved;
static uint32_t picks;
static uint32_t pick_what;
static uint32_t asks;
static uint32_t answer;
static int put_fails;

static const char *F(const char *name)
{
    static char paths[8][512];
    static uint32_t turn;
    char *p = paths[turn++ % 8u];

    snprintf(p, 512u, "%s/files/%s", base, name);

    return p;
}

static long slurp(const char *path, uint8_t *out, size_t max)
{
    FILE *f = fopen(path, "rb");
    long n;

    if (!f)
    {
        return -1;
    }

    n = (long)fread(out, 1u, max, f);
    fclose(f);

    return n;
}

static void spill(const char *path, const void *bytes, size_t n)
{
    FILE *f = fopen(path, "wb");

    fwrite(bytes, 1u, n, f);
    fclose(f);
}

int ed_file_stat(const char *path, uint64_t *size, uint64_t *mtime)
{
    static uint8_t bytes[65536];
    long n = slurp(path, bytes, sizeof bytes);

    if (n < 0)
    {
        return -1;
    }

    *size = (uint64_t)n;
    *mtime = sess_crc(0u, bytes, (uint32_t)n) + 1u;

    return 0;
}

int ed_file_get(const char *path, void *buf, uint32_t n)
{
    return slurp(path, buf, n) == (long)n ? 0 : -1;
}

int ed_file_put(const char *path, const void *buf, uint32_t n)
{
    if (put_fails)
    {
        put_fails = 0;

        return -1;
    }

    spill(path, buf, n);

    return 0;
}

int ed_clip_put(const char *utf8, uint32_t n)
{
    memcpy(clipboard, utf8, n);
    clipboard_n = n;

    return 0;
}

const char *ed_clip_get(uint32_t *n)
{
    *n = clipboard_n;

    return clipboard_n ? clipboard : 0;
}

void ed_pick(uint32_t what)
{
    picks++;
    pick_what = what;
}

uint32_t ed_ask_close(const char *name)
{
    (void)name;
    asks++;

    return answer;
}

static uint32_t exits;
static uint32_t erases;

const int ed_erase_offered = 1;

void ed_erase(void)
{
    erases++;
}

void ed_exit(void)
{
    exits++;
}

void ed_title(const char *name, int unsaved)
{
    snprintf(title, sizeof title, "%s", name);
    title_unsaved = unsaved;
}

static uint16_t wide[8192];

static uint32_t w(const char *s)
{
    return ed_utf8_units(s, (uint32_t)strlen(s), wide, 8192u);
}

static void type(const char *s)
{
    ed_text(s, (uint32_t)strlen(s));
}

static void key(uint32_t k, uint32_t mods)
{
    ed_key(k, mods);
}

static void keys(uint32_t k, uint32_t mods, uint32_t times)
{
    while (times--)
    {
        ed_key(k, mods);
    }
}

static int text_is(const char *s)
{
    const uint16_t *run;
    uint32_t len = w(s);
    uint32_t off;
    uint32_t n;

    if (!ed_now->loaded || hl_len(ed_now->doc) != len)
    {
        return 0;
    }

    for (off = 0u; off < len; off += n)
    {
        run = hl_text(ed_now->doc, off, &n);

        if (!n || memcmp(run, wide + off, n * sizeof wide[0]))
        {
            return 0;
        }
    }

    return 1;
}

static int sel_is(uint32_t anchor, uint32_t caret)
{
    return ed_now->anchor == anchor && ed_now->caret == caret;
}

static void frame(void)
{
    rect_n = 0u;
    glyph_n = 0u;
    ed_draw();
}

static const struct glyph *glyph_at(int32_t x, int32_t y)
{
    uint32_t i;

    for (i = glyph_n; i; i--)
    {
        if (x >= glyphs[i - 1u].x && x < glyphs[i - 1u].x + glyphs[i - 1u].w && y >= glyphs[i - 1u].y && y < glyphs[i - 1u].y + glyphs[i - 1u].h)
        {
            return &glyphs[i - 1u];
        }
    }

    return 0;
}

static uint32_t back_at(int32_t x, int32_t y)
{
    uint32_t i;

    for (i = rect_n; i; i--)
    {
        if (x >= rects[i - 1u].x && x < rects[i - 1u].x + rects[i - 1u].w && y >= rects[i - 1u].y && y < rects[i - 1u].y + rects[i - 1u].h)
        {
            return rects[i - 1u].rgb;
        }
    }

    return 0xFF000000u;
}

static uint32_t cell_char(uint32_t row, uint32_t col)
{
    const struct glyph *g = glyph_at(TEXT_X + (int32_t)col * FW + 2, TEXT_Y + (int32_t)row * LH + 2);

    return g ? g->unit : 0u;
}

static uint32_t cell_ink(uint32_t row, uint32_t col)
{
    const struct glyph *g = glyph_at(TEXT_X + (int32_t)col * FW + 2, TEXT_Y + (int32_t)row * LH + 2);

    return g ? g->rgb : 0xFF000000u;
}

static uint32_t cell_back(uint32_t row, uint32_t col)
{
    return back_at(TEXT_X + (int32_t)col * FW + 3, TEXT_Y + (int32_t)row * LH + 8);
}

static void click(uint32_t row, uint32_t col, uint32_t mods, uint32_t clicks)
{
    ed_mouse_down(TEXT_X + (int32_t)col * FW + 1, TEXT_Y + (int32_t)row * LH + 8, mods, clicks);
    ed_mouse_up();
}

static const char *band(int32_t y)
{
    static char s[512];
    int32_t x;
    uint32_t n = 0u;
    const struct glyph *g;

    for (x = 0; x < ed_w && n < 500u; x += FW / 2)
    {
        g = glyph_at(x, y);

        if (g && g->x >= x - FW / 2 + 1 && g->x <= x && !(g->unit == ' ' && n && s[n - 1u] == ' '))
        {
            s[n++] = g->unit < 128u ? (char)g->unit : '?';
        }

        else if (!g && n && s[n - 1u] != ' ')
        {
            s[n++] = ' ';
        }
    }

    while (n && s[n - 1u] == ' ')
    {
        n--;
    }

    s[n] = 0;

    return s;
}

static const char *status(void)
{
    const char *s;

    ed_resize(1280, H, 100);
    frame();
    s = band(H - STATUS_H + 8);
    ed_resize(W, H, 100);
    frame();

    return s;
}

static const char *bar(void)
{
    return band(BAR_Y + 12);
}

static int32_t bar_x(uint32_t what, uint32_t pos)
{
    uint32_t got;
    int32_t x;

    for (x = 0; x < W; x++)
    {
        if (ed_hit(x, BAR_Y + 10, &got) == what && (what == ED_HIT_TAB_NEW || got == pos))
        {
            return x;
        }
    }

    return -1;
}

static uint32_t syntax(const char *name)
{
    uint32_t i;

    for (i = 0u; i < hl_theme_count; i++)
    {
        if (!strcmp(hl_theme[i], name))
        {
            return ed_syntax[i];
        }
    }

    return 0xFF000000u;
}

static int file_is(const char *path, const void *bytes, size_t n)
{
    static uint8_t got[65536];

    return slurp(path, got, sizeof got) == (long)n && !memcmp(got, bytes, n);
}

static void scratch(const char *s)
{
    key('n', ED_MOD_CTRL);
    type(s);
}

static void t_start(void)
{
    puts("a fresh start: one untitled tab, an empty text, a frame that says so");
    CHECK(sess_count() == 1u && ed_now && ed_now->loaded && text_is(""));
    CHECK(!strcmp(title, "new 1") && !title_unsaved);
    CHECK(ed_stale());
    frame();
    CHECK(!ed_stale());
    CHECK(!strcmp(bar(), "new 1 x +"));
    CHECK(!strcmp(status(), "Normal text file length: 0 lines: 1 Ln: 1 Col: 1 Pos: 1 Unix (LF) UTF-8 INS"));

    puts("in a narrow window the length gives way and the caret's place stays");
    CHECK(!strcmp(band(H - STATUS_H + 8), "Normal text file Ln: 1 Col: 1 Pos: 1 Unix (LF) UTF-8 INS"));
    CHECK(glyph_at(36 - 6 - FW + 1, TEXT_Y + 2) && glyph_at(36 - 6 - FW + 1, TEXT_Y + 2)->unit == '1');
    CHECK(back_at(TEXT_X + 1, TEXT_Y + 3) == ed_colours[ED_C_CARET]);
    CHECK(back_at(TEXT_X + 100, TEXT_Y + 3) == ed_colours[ED_C_LINE]);
    CHECK(back_at(TEXT_X + 100, TEXT_Y + LH + 3) == ed_colours[ED_C_BACK]);
}

static void t_typing(void)
{
    puts("typing puts text at the caret and the tab becomes unsaved");
    type("int x;");
    CHECK(text_is("int x;") && sel_is(6u, 6u));
    CHECK(sess_unsaved(ed_cur));
    key(ED_KEY_ENTER, 0u);
    type("y");
    CHECK(text_is("int x;\ny") && sel_is(8u, 8u));
    frame();
    CHECK(cell_char(0u, 0u) == 'i' && cell_char(0u, 4u) == 'x' && cell_char(1u, 0u) == 'y');
    CHECK(!strcmp(bar(), "new 1 * x +"));
    CHECK(!strcmp(status(), "Normal text file length: 8 lines: 2 Ln: 2 Col: 2 Pos: 9 Unix (LF) UTF-8 INS"));
    CHECK(back_at(TEXT_X + FW + 1, TEXT_Y + LH + 3) == ed_colours[ED_C_CARET]);

    puts("enter carries the row's indentation down, and a tab is a tab");
    key(ED_KEY_ENTER, 0u);
    key(ED_KEY_TAB, 0u);
    type("  a");
    key(ED_KEY_ENTER, 0u);
    type("b");
    CHECK(text_is("int x;\ny\n\t  a\n\t  b"));
    frame();
    CHECK(cell_char(2u, 0u) == ' ' && cell_char(2u, 6u) == 'a' && cell_char(3u, 6u) == 'b');
}

static void t_moving(void)
{
    puts("the arrows, home and end, by character and by word");
    keys(ED_KEY_UP, 0u, 9u);
    CHECK(sel_is(0u, 0u));
    key(ED_KEY_RIGHT, 0u);
    CHECK(sel_is(1u, 1u));
    key(ED_KEY_RIGHT, ED_MOD_CTRL);
    CHECK(sel_is(4u, 4u));
    key(ED_KEY_RIGHT, ED_MOD_CTRL);
    CHECK(sel_is(5u, 5u));
    key(ED_KEY_END, 0u);
    CHECK(sel_is(6u, 6u));
    key(ED_KEY_LEFT, ED_MOD_CTRL);
    CHECK(sel_is(5u, 5u));
    key(ED_KEY_LEFT, ED_MOD_CTRL);
    CHECK(sel_is(4u, 4u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(0u, 0u));
    key(ED_KEY_END, ED_MOD_CTRL);
    CHECK(sel_is(18u, 18u));

    puts("home goes to the first thing that is not a blank, then to the margin");
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(17u, 17u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(14u, 14u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(17u, 17u));

    puts("up and down keep the column they started from across a shorter row");
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key(ED_KEY_END, 0u);
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(8u, 8u));
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(12u, 12u));
    key(ED_KEY_UP, 0u);
    key(ED_KEY_UP, 0u);
    CHECK(sel_is(6u, 6u));

    puts("shift extends a selection, and a plain arrow collapses it to its near end");
    key(ED_KEY_HOME, ED_MOD_SHIFT);
    CHECK(sel_is(6u, 0u));
    frame();
    CHECK(cell_back(0u, 2u) == ed_colours[ED_C_SELECTION] && cell_back(0u, 7u) != ed_colours[ED_C_SELECTION]);
    key(ED_KEY_RIGHT, 0u);
    CHECK(sel_is(6u, 6u));
    key(ED_KEY_DOWN, ED_MOD_SHIFT);
    CHECK(sel_is(6u, 8u));
    frame();
    CHECK(cell_back(0u, 6u) == ed_colours[ED_C_SELECTION] && cell_back(1u, 0u) == ed_colours[ED_C_SELECTION]);
    key(ED_KEY_LEFT, 0u);
    CHECK(sel_is(6u, 6u));
    key('a', ED_MOD_CTRL);
    CHECK(sel_is(0u, 18u));
}

static void t_deleting(void)
{
    puts("backspace and delete, by character, by word, and over a selection");
    type("one two  three");
    CHECK(text_is("one two  three"));
    key(ED_KEY_BACKSPACE, 0u);
    CHECK(text_is("one two  thre"));
    key(ED_KEY_BACKSPACE, ED_MOD_CTRL);
    CHECK(text_is("one two  "));
    key(ED_KEY_BACKSPACE, ED_MOD_CTRL);
    CHECK(text_is("one "));
    key(ED_KEY_HOME, 0u);
    key(ED_KEY_DELETE, 0u);
    CHECK(text_is("ne "));
    key(ED_KEY_DELETE, ED_MOD_CTRL);
    CHECK(text_is(""));
    key(ED_KEY_BACKSPACE, 0u);
    key(ED_KEY_DELETE, 0u);
    CHECK(text_is("") && sel_is(0u, 0u));
    type("abcdef");
    keys(ED_KEY_LEFT, ED_MOD_SHIFT, 3u);
    key(ED_KEY_BACKSPACE, 0u);
    CHECK(text_is("abc"));
    keys(ED_KEY_LEFT, ED_MOD_SHIFT, 2u);
    type("X");
    CHECK(text_is("aX") && sel_is(2u, 2u));
}

static void t_undo(void)
{
    puts("undo takes typing back a run at a time, and redo puts it forward");
    scratch("");
    type("h");
    type("e");
    type("y");
    key(ED_KEY_ENTER, 0u);
    type("y");
    type("o");
    CHECK(text_is("hey\nyo"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("hey\n") && sel_is(4u, 4u));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("hey"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("") && sel_is(0u, 0u));
    key('z', ED_MOD_CTRL);
    CHECK(text_is(""));
    key('y', ED_MOD_CTRL);
    key('z', ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("hey\n"));
    key('y', ED_MOD_CTRL);
    CHECK(text_is("hey\nyo") && sel_is(6u, 6u));
    key('y', ED_MOD_CTRL);
    CHECK(text_is("hey\nyo"));

    puts("moving the caret ends a run, and a new edit drops what could be redone");
    type("a");
    key(ED_KEY_LEFT, 0u);
    key(ED_KEY_RIGHT, 0u);
    type("b");
    key('z', ED_MOD_CTRL);
    CHECK(text_is("hey\nyoa"));
    type("c");
    key('y', ED_MOD_CTRL);
    CHECK(text_is("hey\nyoac"));

    puts("undoing a replaced selection brings back the text and the selection");
    key('a', ED_MOD_CTRL);
    type("Z");
    CHECK(text_is("Z"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("hey\nyoac") && sel_is(0u, 8u));
}

static void t_clipboard(void)
{
    puts("copy, cut and paste, with line ends folded on the way in");
    scratch("alpha beta");
    keys(ED_KEY_LEFT, ED_MOD_SHIFT, 4u);
    key('c', ED_MOD_CTRL);
    CHECK(clipboard_n == 4u && !memcmp(clipboard, "beta", 4u));
    key(ED_KEY_HOME, 0u);
    key('v', ED_MOD_CTRL);
    CHECK(text_is("betaalpha beta") && sel_is(4u, 4u));
    keys(ED_KEY_RIGHT, ED_MOD_SHIFT, 5u);
    key('x', ED_MOD_CTRL);
    CHECK(text_is("beta beta") && clipboard_n == 5u && !memcmp(clipboard, "alpha", 5u));
    ed_clip_put("1\r\n2\r3\n", 7u);
    key('a', ED_MOD_CTRL);
    key('v', ED_MOD_CTRL);
    CHECK(text_is("1\n2\n3\n"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("beta beta"));
    clipboard_n = 0u;
    key('v', ED_MOD_CTRL);
    CHECK(text_is("beta beta"));
}

static void t_unicode(void)
{
    puts("a character outside the first plane is two units and one step");
    scratch("a\xC3\xA9\xF0\x9F\x98\x80z");
    CHECK(hl_len(ed_now->doc) == 5u && sel_is(5u, 5u));
    key(ED_KEY_LEFT, 0u);
    key(ED_KEY_LEFT, 0u);
    CHECK(sel_is(2u, 2u));
    key(ED_KEY_RIGHT, 0u);
    CHECK(sel_is(4u, 4u));
    key(ED_KEY_BACKSPACE, 0u);
    CHECK(text_is("a\xC3\xA9z") && sel_is(2u, 2u));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("a\xC3\xA9\xF0\x9F\x98\x80z"));
    click(0u, 2u, 0u, 1u);
    CHECK(sel_is(2u, 2u));
    click(0u, 3u, 0u, 1u);
    CHECK(sel_is(4u, 4u));
    key(ED_KEY_END, 0u);
    keys(ED_KEY_LEFT, ED_MOD_SHIFT, 2u);
    key('c', ED_MOD_CTRL);
    CHECK(clipboard_n == 5u && !memcmp(clipboard, "\xF0\x9F\x98\x80z", 5u));
}

static void t_mouse(void)
{
    puts("a click puts the caret on the nearest gap, a shift click selects to it");
    scratch("first row\n\tsecond row\nthird");
    click(0u, 3u, 0u, 1u);
    CHECK(sel_is(3u, 3u));
    ed_mouse_down(TEXT_X + 3 * FW + 6, TEXT_Y + 8, 0u, 1u);
    ed_mouse_up();
    CHECK(sel_is(4u, 4u));
    click(2u, 2u, ED_MOD_SHIFT, 1u);
    CHECK(sel_is(4u, 24u));
    click(0u, 50u, 0u, 1u);
    CHECK(sel_is(9u, 9u));
    click(9u, 0u, 0u, 1u);
    CHECK(sel_is(22u, 22u));

    puts("a tab is one stop wide under the pointer");
    click(1u, 1u, 0u, 1u);
    CHECK(sel_is(10u, 10u));
    click(1u, 3u, 0u, 1u);
    CHECK(sel_is(11u, 11u));
    click(1u, 5u, 0u, 1u);
    CHECK(sel_is(12u, 12u));
    frame();
    CHECK(cell_char(1u, 4u) == 's' && cell_back(1u, 0u) == ed_colours[ED_C_LINE]);

    puts("two clicks take a word, three take the row, a drag takes what it crosses");
    click(0u, 7u, 0u, 2u);
    CHECK(sel_is(6u, 9u));
    click(1u, 7u, 0u, 3u);
    CHECK(sel_is(10u, 22u));
    ed_mouse_down(TEXT_X + 2 * FW + 1, TEXT_Y + 8, 0u, 1u);
    ed_mouse_move(TEXT_X + 6 * FW + 1, TEXT_Y + 2 * LH + 8);
    CHECK(sel_is(2u, 27u));
    ed_mouse_up();
    ed_mouse_move(TEXT_X, TEXT_Y);
    CHECK(sel_is(2u, 27u));
}

static void t_scrolling(void)
{
    char line[32];
    uint32_t i;

    puts("the view follows the caret down a long text and back");
    scratch("");

    for (i = 1u; i <= 100u; i++)
    {
        snprintf(line, sizeof line, "row %u\n", (unsigned)i);
        type(line);
    }

    CHECK(hl_rows(ed_now->doc) == 101u && ed_now->top == 101u - ROWS);
    frame();
    CHECK(cell_char(ROWS - 2u, 4u) == '1' && cell_char(ROWS - 2u, 6u) == '0');
    CHECK(glyph_at(36 - 6 - 3 * FW + 1, TEXT_Y + (ROWS - 1) * LH + 2)->unit == '1');
    key(ED_KEY_HOME, ED_MOD_CTRL);
    CHECK(ed_now->top == 0u);
    key(ED_KEY_PAGE_DOWN, 0u);
    CHECK(sel_is(6u * 9u + 5u * 7u, 6u * 9u + 5u * 7u) && ed_now->top == 0u);
    key(ED_KEY_PAGE_DOWN, 0u);
    CHECK(ed_now->top == 28u - ROWS + 1u);
    key(ED_KEY_PAGE_UP, 0u);
    key(ED_KEY_PAGE_UP, 0u);
    key(ED_KEY_PAGE_UP, 0u);
    CHECK(sel_is(0u, 0u));

    puts("the wheel and the scroll bar move the view and leave the caret");
    ed_wheel(2, 0u);
    CHECK(ed_now->top == 6u && sel_is(0u, 0u));
    ed_wheel(-100, 0u);
    CHECK(ed_now->top == 0u);
    ed_wheel(1000, 0u);
    CHECK(ed_now->top == 100u);
    ed_mouse_down(W - 5, TEXT_Y + 125, 0u, 1u);
    CHECK(ed_now->top == 50u && sel_is(0u, 0u));
    ed_mouse_move(W - 5, TEXT_Y);
    CHECK(ed_now->top == 0u);
    ed_mouse_up();

    puts("a row longer than the window scrolls sideways to keep the caret in sight");
    scratch("");

    for (i = 0u; i < 20u; i++)
    {
        type("0123456789");
    }

    CHECK(ed_now->left > 0);
    frame();
    CHECK(back_at(TEXT_X + 200 * FW - ed_now->left + 1, TEXT_Y + 3) == ed_colours[ED_C_CARET]);
    CHECK(glyph_at(TEXT_X - 3, TEXT_Y + 2) == 0);
    key(ED_KEY_HOME, 0u);
    CHECK(ed_now->left == 0);
}

static void t_files(void)
{
    static const char c_crlf[] = "#include <a.h>\r\nint main(void)\r\n{\r\n    return 42; /* ok */\r\n}\r\n";
    static const uint8_t bom[] = { 0xEF, 0xBB, 0xBF, 'h', 'i', '\n' };
    static const uint8_t le[] = { 0xFF, 0xFE, 'h', 0, 0x3D, 0xD8, 0x00, 0xDE, '\r', 0, '\n', 0 };
    static const uint8_t be[] = { 0xFE, 0xFF, 0, 'h', 0xD8, 0x3D, 0xDE, 0x00, 0, '\r' };
    static const uint8_t latin[] = { 'c', 'a', 'f', 0xE9, '\n', 0xFF, 0x80 };
    uint32_t tabs = sess_count();

    puts("a C file opens in a tab of its own, coloured, with its line ends remembered");
    spill(F("m.c"), c_crlf, sizeof c_crlf - 1u);
    CHECK(!ed_open(F("m.c")));
    CHECK(sess_count() == tabs + 1u && !strcmp(title, "m.c") && !title_unsaved);
    CHECK(text_is("#include <a.h>\nint main(void)\n{\n    return 42; /* ok */\n}\n"));
    frame();
    CHECK(!strcmp(status(), "C source file length: 63 lines: 6 Ln: 1 Col: 1 Pos: 1 Windows (CR LF) UTF-8 INS"));
    CHECK(cell_ink(0u, 0u) == syntax("keyword") && cell_ink(0u, 9u) == syntax("string"));
    CHECK(cell_ink(1u, 0u) == syntax("type") && cell_ink(1u, 4u) == syntax("function"));
    CHECK(cell_ink(3u, 4u) == syntax("keyword") && cell_ink(3u, 11u) == syntax("number") && cell_ink(3u, 15u) == syntax("comment"));
    CHECK(cell_ink(2u, 0u) == ed_colours[ED_C_TEXT]);

    puts("opening it again shows the same tab");
    key('n', ED_MOD_CTRL);
    CHECK(!ed_open(F("m.c")) && sess_count() == tabs + 2u && !strcmp(title, "m.c"));

    puts("an edit is coloured as it is typed, and a save writes the file's own line ends");
    key(ED_KEY_END, ED_MOD_CTRL);
    type("int FOO;");
    frame();
    CHECK(cell_ink(5u, 0u) == syntax("type") && cell_ink(5u, 4u) == syntax("constant"));
    CHECK(title_unsaved);
    key('s', ED_MOD_CTRL);
    CHECK(!title_unsaved && !sess_unsaved(ed_cur));
    CHECK(file_is(F("m.c"), "#include <a.h>\r\nint main(void)\r\n{\r\n    return 42; /* ok */\r\n}\r\nint FOO;", sizeof c_crlf - 1u + 8u));

    puts("a save that fails says so and the tab stays unsaved");
    type("!");
    put_fails = 1;
    key('s', ED_MOD_CTRL);
    frame();
    CHECK(sess_unsaved(ed_cur) && strstr(status(), "could not save"));
    key('s', ED_MOD_CTRL);
    frame();
    CHECK(!sess_unsaved(ed_cur) && !strstr(status(), "could not save"));

    puts("every encoding goes through a load and a save byte for byte");
    spill(F("bom.txt"), bom, sizeof bom);
    spill(F("le.txt"), le, sizeof le);
    spill(F("be.txt"), be, sizeof be);
    spill(F("latin.txt"), latin, sizeof latin);
    CHECK(!ed_open(F("bom.txt")) && text_is("hi\n"));
    frame();
    CHECK(strstr(status(), "length: 3 lines: 2") && strstr(status(), "Unix (LF) UTF-8-BOM INS"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("bom.txt"), bom, sizeof bom));
    CHECK(!ed_open(F("le.txt")) && text_is("h\xF0\x9F\x98\x80\n"));
    frame();
    CHECK(strstr(status(), "length: 7 lines: 2") && strstr(status(), "Windows (CR LF) UTF-16 LE BOM INS"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("le.txt"), le, sizeof le));
    CHECK(!ed_open(F("be.txt")) && text_is("h\xF0\x9F\x98\x80\n"));
    frame();
    CHECK(strstr(status(), "length: 6 lines: 2") && strstr(status(), "Macintosh (CR) UTF-16 BE BOM INS"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("be.txt"), be, sizeof be));
    CHECK(!ed_open(F("latin.txt")) && text_is("caf\xC3\xA9\n\xC3\xBF\xC2\x80"));
    frame();
    CHECK(strstr(status(), "length: 10 lines: 2") && strstr(status(), "Unix (LF) ANSI INS"));
    key(ED_KEY_END, ED_MOD_CTRL);
    type("!");
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("latin.txt"), "caf\xE9\n\xFF\x80!", 8u));

    puts("an untitled tab asks where to go, and takes its language from the answer");
    scratch("int n;");
    frame();
    CHECK(cell_ink(0u, 0u) == ed_colours[ED_C_TEXT]);
    picks = 0u;
    key('s', ED_MOD_CTRL);
    CHECK(picks == 1u && pick_what == ED_PICK_SAVE && sess_unsaved(ed_cur));
    ed_picked(ED_PICK_SAVE, 0);
    CHECK(sess_unsaved(ed_cur) && !sess_tab_path(ed_cur));
    key('s', ED_MOD_CTRL);
    ed_picked(ED_PICK_SAVE, F("n.c"));
    CHECK(!sess_unsaved(ed_cur) && !strcmp(title, "n.c") && file_is(F("n.c"), "int n;", 6u));
    frame();
    CHECK(cell_ink(0u, 0u) == syntax("type") && strstr(status(), "length:"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is(""));
    key('y', ED_MOD_CTRL);
    CHECK(text_is("int n;"));

    puts("a path with no file behind it opens empty and a save makes the file");
    remove(F("fresh.c"));
    CHECK(!ed_open(F("fresh.c")) && text_is(""));
    frame();
    CHECK(strstr(status(), "not on disk yet"));
    type("x");
    key('s', ED_MOD_CTRL);
    frame();
    CHECK(file_is(F("fresh.c"), "x", 1u) && !strstr(status(), "not on disk yet"));

    puts("ctrl+o asks for a file and opens the answer");
    picks = 0u;
    key('o', ED_MOD_CTRL);
    CHECK(picks == 1u && pick_what == ED_PICK_OPEN);
    ed_picked(ED_PICK_OPEN, F("bom.txt"));
    CHECK(!strcmp(title, "bom.txt"));
}

static void t_tabs(void)
{
    uint32_t n;
    uint32_t first;
    int32_t x;

    puts("ctrl+tab walks the tabs round, and a click in the bar picks one");
    n = sess_count();
    first = sess_active();
    key(ED_KEY_TAB, ED_MOD_CTRL);
    CHECK(sess_active() == (first + 1u) % n);
    key(ED_KEY_TAB, ED_MOD_CTRL | ED_MOD_SHIFT);
    key(ED_KEY_TAB, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(sess_active() == (first + n - 1u) % n);
    ed_tab_show(0u);
    frame();
    x = bar_x(ED_HIT_TAB, 1u);
    CHECK(x > 0);
    ed_mouse_down(x, BAR_Y + 10, 0u, 1u);
    ed_mouse_up();
    CHECK(sess_active() == 1u);

    puts("the bar scrolls to keep the showing tab in it");
    ed_tab_show(n - 1u);
    frame();
    CHECK(bar_x(ED_HIT_TAB, n - 1u) >= 0 && bar_x(ED_HIT_TAB, 0u) < 0);
    ed_tab_show(0u);
    frame();
    CHECK(bar_x(ED_HIT_TAB, 0u) == 0);

    puts("closing a clean tab asks nothing; closing an unsaved one asks, and the answer is obeyed");
    CHECK(!ed_open(F("bom.txt")));
    asks = 0u;
    key('w', ED_MOD_CTRL);
    CHECK(asks == 0u && sess_count() == n - 1u);
    n = sess_count();
    scratch("keep me");
    answer = ED_CLOSE_CANCEL;
    key('w', ED_MOD_CTRL);
    CHECK(asks == 1u && sess_count() == n + 1u && text_is("keep me"));
    answer = ED_CLOSE_SAVE;
    picks = 0u;
    key('w', ED_MOD_CTRL);
    CHECK(asks == 2u && picks == 1u && sess_count() == n + 1u);
    ed_picked(ED_PICK_SAVE, F("kept.txt"));
    CHECK(sess_count() == n && file_is(F("kept.txt"), "keep me", 7u));
    scratch("drop me");
    answer = ED_CLOSE_DISCARD;
    frame();
    x = bar_x(ED_HIT_TAB_CLOSE, sess_active());
    CHECK(x > 0);
    ed_mouse_down(x, BAR_Y + 10, 0u, 1u);
    ed_mouse_up();
    CHECK(asks == 3u && sess_count() == n);

    puts("the + in the bar makes a tab, and closing the last tab leaves a fresh one");
    ed_tab_show(sess_count() - 1u);
    frame();
    x = bar_x(ED_HIT_TAB_NEW, 0u);
    CHECK(x > 0);
    ed_mouse_down(x, BAR_Y + 10, 0u, 1u);
    ed_mouse_up();
    CHECK(sess_count() == n + 1u && sess_active() == n && text_is(""));

    while (sess_count() > 1u)
    {
        key('w', ED_MOD_CTRL);
    }

    key('w', ED_MOD_CTRL);
    key('w', ED_MOD_CTRL);
    CHECK(sess_count() == 1u && text_is("") && !sess_tab_path(ed_cur) && !strcmp(title, "new 1"));

    puts("opening a file into an untouched blank tab takes its place");
    CHECK(!ed_open(F("m.c")) && sess_count() == 1u && !strcmp(title, "m.c"));
}

static const int32_t menu_x[] = { 2, 54, 106, 174, 226, 310, 394 };

enum { M_FILE, M_EDIT, M_SEARCH, M_VIEW, M_ENCODING, M_LANGUAGE, M_WINDOW };

static void click_at(int32_t x, int32_t y)
{
    ed_mouse_move(x, y);
    ed_mouse_down(x, y, 0u, 1u);
    ed_mouse_up();
}

static void shut_menu(void)
{
    if (ed_menu_open())
    {
        key(ED_KEY_ESCAPE, 0u);
    }
}

static const struct glyph *run_of(const char *text)
{
    uint32_t n = (uint32_t)strlen(text);
    uint32_t i;
    uint32_t k;

    for (i = glyph_n; i; i--)
    {
        if (glyphs[i - 1u].run != n || i - 1u + n > glyph_n)
        {
            continue;
        }

        for (k = 0u; k < n && glyphs[i - 1u + k].unit == (uint8_t)text[k]; k++)
        {
        }

        if (k == n)
        {
            return &glyphs[i - 1u];
        }
    }

    return 0;
}

static const struct glyph *item_of(uint32_t menu, const char *label)
{
    shut_menu();
    click_at(menu_x[menu] + 5, 10);
    frame();

    return run_of(label);
}

static int choose(uint32_t menu, const char *label)
{
    const struct glyph *g = item_of(menu, label);

    if (!g)
    {
        shut_menu();

        return 0;
    }

    click_at(g->x + 8, g->y + LH / 2);

    return 1;
}

static uint32_t item_ink(uint32_t menu, const char *label)
{
    const struct glyph *g = item_of(menu, label);

    shut_menu();

    return g ? g->rgb : 0xFF000000u;
}

static int item_ticked(uint32_t menu, const char *label)
{
    const struct glyph *g = item_of(menu, label);
    int ticked = g && back_at(g->x - 11, g->y + 8) == ed_colours[ED_C_MENU_TEXT];

    shut_menu();

    return ticked;
}

#define TOOL_NEW 6
#define TOOL_SAVE 58
#define TOOL_UNDO 258
#define TOOL_FIND 319
#define TOOL_ZOOM_IN 380
#define TOOL_SPACES 441

static void tool(int32_t x)
{
    click_at(x + 12, TOOL_Y + 14);
}

static void only_one(void)
{
    answer = ED_CLOSE_DISCARD;
    choose(M_FILE, "Close All");
}

static void t_names(void)
{
    puts("untitled tabs are numbered, and a number that is free is used again");
    only_one();
    CHECK(sess_count() == 1u && !strcmp(title, "new 1"));
    key('n', ED_MOD_CTRL);
    key('n', ED_MOD_CTRL);
    CHECK(!strcmp(title, "new 3"));
    frame();
    CHECK(!strcmp(bar(), "new 1 x new 2 x new 3 x +"));
    ed_tab_show(1u);
    key('w', ED_MOD_CTRL);
    key('n', ED_MOD_CTRL);
    CHECK(!strcmp(title, "new 2"));
    only_one();
}

static void t_menus(void)
{
    uint32_t n;

    puts("a click on a title opens its menu, which then has the keyboard and the mouse");
    scratch("abc");
    n = sess_count();
    click_at(menu_x[M_FILE] + 5, 10);
    CHECK(ed_menu_open());
    frame();
    CHECK(strstr(band(MENU_H + 8), "New Ctrl+N"));
    CHECK(back_at(menu_x[M_FILE] + 4, 8) == ed_colours[ED_C_MENU_HOT]);
    type("ignored");
    key('n', ED_MOD_CTRL);
    CHECK(text_is("abc") && sess_count() == n && ed_menu_open());

    puts("the item under the pointer is lit, and a click outside shuts the menu and does nothing else");
    ed_mouse_move(menu_x[M_FILE] + 30, MENU_H + 8);
    frame();
    CHECK(back_at(menu_x[M_FILE] + 100, MENU_H + 8) == ed_colours[ED_C_MENU_HOT]);
    click_at(W - 60, TEXT_Y + 8);
    CHECK(!ed_menu_open() && sel_is(3u, 3u));
    click_at(menu_x[M_EDIT] + 5, 10);
    ed_mouse_move(menu_x[M_VIEW] + 5, 10);
    frame();
    CHECK(ed_menu_open() && strstr(band(MENU_H + 8), "Toolbar"));
    click_at(menu_x[M_VIEW] + 5, 10);
    CHECK(!ed_menu_open());

    puts("choosing an item does what its key does");
    CHECK(choose(M_FILE, "New") && sess_count() == n + 1u && !ed_menu_open());
    key('w', ED_MOD_CTRL);
    CHECK(choose(M_EDIT, "Select All") && sel_is(0u, 3u));
    CHECK(choose(M_EDIT, "Copy") && clipboard_n == 3u);
    picks = 0u;
    CHECK(choose(M_FILE, "Save As...") && picks == 1u && pick_what == ED_PICK_SAVE);
    ed_picked(ED_PICK_SAVE, 0);
    exits = 0u;
    CHECK(choose(M_FILE, "Exit") && exits == 1u);

    puts("what cannot be done is grey, and choosing it does nothing");
    key('n', ED_MOD_CTRL);
    CHECK(item_ink(M_EDIT, "Undo") == ed_colours[ED_C_DIM]);
    CHECK(item_ink(M_EDIT, "Paste") == ed_colours[ED_C_MENU_TEXT]);
    CHECK(item_ink(M_FILE, "Reload from Disk") == ed_colours[ED_C_DIM]);
    CHECK(choose(M_EDIT, "Undo") && ed_menu_open());
    shut_menu();
    type("x");
    CHECK(item_ink(M_EDIT, "Undo") == ed_colours[ED_C_MENU_TEXT]);
    CHECK(choose(M_EDIT, "Undo") && text_is(""));
    key('w', ED_MOD_CTRL);

    puts("Alt and a letter opens a menu, and the arrows and Enter work it");
    key('e', ED_MOD_ALT);
    CHECK(ed_menu_open());
    key(ED_KEY_ENTER, 0u);
    CHECK(!ed_menu_open() && text_is(""));
    key('y', ED_MOD_CTRL);
    CHECK(text_is("abc"));
    key('v', ED_MOD_ALT);
    key(ED_KEY_DOWN, 0u);
    key(ED_KEY_ENTER, 0u);
    CHECK(!ed_opt.status);
    key('v', ED_MOD_ALT);
    key(ED_KEY_UP, 0u);
    key(ED_KEY_UP, 0u);
    key(ED_KEY_DOWN, 0u);
    key(ED_KEY_DOWN, 0u);
    key(ED_KEY_DOWN, 0u);
    key(ED_KEY_ENTER, 0u);
    CHECK(ed_opt.status);
    key('f', ED_MOD_ALT);
    key(ED_KEY_RIGHT, 0u);
    frame();
    CHECK(strstr(band(MENU_H + 8), "Undo"));
    key(ED_KEY_LEFT, 0u);
    key(ED_KEY_LEFT, 0u);
    frame();
    CHECK(strstr(band(MENU_H + 8), "Next Tab"));
    key(ED_KEY_ESCAPE, 0u);
    CHECK(!ed_menu_open());

    puts("a menu taller than the window goes on in a second column, and stays inside it");
    CHECK(item_of(M_EDIT, "Overwrite Mode") && item_of(M_EDIT, "Overwrite Mode")->y + LH < H);
    CHECK(item_of(M_EDIT, "Overwrite Mode")->x > item_of(M_EDIT, "Undo")->x);
    CHECK(item_of(M_FILE, "Exit")->x == item_of(M_FILE, "New")->x);
    shut_menu();

    puts("the Window menu lists the tabs and goes to the one chosen");
    scratch("second");
    CHECK(choose(M_WINDOW, "new 2") && text_is("abc"));
    CHECK(choose(M_WINDOW, "Next Tab") && text_is("second"));
    only_one();
}

static void t_toolbar(void)
{
    uint32_t n = sess_count();

    puts("a toolbar button is its command, and is grey when the command is");
    frame();
    CHECK(back_at(TOOL_UNDO + 4 + 5, TOOL_Y + 2 + 4 + 4) == ed_colours[ED_C_DIM]);
    CHECK(back_at(TOOL_NEW + 4 + 3, TOOL_Y + 2 + 4 + 1) == ed_colours[ED_C_MENU_TEXT]);
    tool(TOOL_NEW);
    CHECK(sess_count() == n + 1u);
    type("typed");
    frame();
    CHECK(back_at(TOOL_UNDO + 4 + 5, TOOL_Y + 2 + 4 + 4) == ed_colours[ED_C_MENU_TEXT]);
    tool(TOOL_UNDO);
    CHECK(text_is(""));
    picks = 0u;
    tool(TOOL_SAVE);
    CHECK(picks == 1u);
    ed_picked(ED_PICK_SAVE, 0);

    puts("the button under the pointer is lit");
    ed_mouse_move(TOOL_NEW + 12, TOOL_Y + 14);
    frame();
    CHECK(back_at(TOOL_NEW + 1, TOOL_Y + 3) == ed_colours[ED_C_MENU_HOT]);
    ed_mouse_move(TEXT_X + 50, TEXT_Y + 50);
    frame();
    CHECK(back_at(TOOL_NEW + 1, TOOL_Y + 3) == ed_colours[ED_C_MENU_BACK]);

    puts("spaces and tabs can be shown, from the toolbar or the View menu, and the two agree");
    type("a b\tc");
    tool(TOOL_SPACES);
    frame();
    CHECK(cell_char(0u, 1u) == 0xB7u && cell_ink(0u, 1u) == ed_colours[ED_C_DIM]);
    CHECK(cell_char(0u, 3u) == 0x2192u && cell_char(0u, 4u) == 'c' && cell_ink(0u, 4u) == ed_colours[ED_C_TEXT]);
    CHECK(back_at(TOOL_SPACES + 1, TOOL_Y + 3) == ed_colours[ED_C_MENU_HOT]);
    CHECK(item_ticked(M_VIEW, "Show Spaces and Tabs"));
    CHECK(choose(M_VIEW, "Show Spaces and Tabs") && !item_ticked(M_VIEW, "Show Spaces and Tabs"));
    frame();
    CHECK(cell_char(0u, 1u) == ' ' && back_at(TOOL_SPACES + 1, TOOL_Y + 3) == ed_colours[ED_C_MENU_BACK]);

    puts("the toolbar, the status line and the line numbers can each be put away");
    CHECK(choose(M_VIEW, "Toolbar"));
    frame();
    CHECK(cell_char(0u, 0u) == 0u && glyph_at(TEXT_X + 2, TEXT_Y - 28 + 2) && glyph_at(TEXT_X + 2, TEXT_Y - 28 + 2)->unit == 'a');
    CHECK(choose(M_VIEW, "Toolbar"));
    CHECK(choose(M_VIEW, "Line Numbers"));
    frame();
    CHECK(glyph_at(6 + 2, TEXT_Y + 2) && glyph_at(6 + 2, TEXT_Y + 2)->unit == 'a');
    CHECK(choose(M_VIEW, "Line Numbers"));
    CHECK(choose(M_VIEW, "Status Bar"));
    frame();
    CHECK(!strcmp(band(H - STATUS_H + 8), ""));
    CHECK(choose(M_VIEW, "Status Bar"));
    frame();
    CHECK(cell_char(0u, 0u) == 'a' && strstr(band(H - STATUS_H + 8), "INS"));
    only_one();
}

static int press(const char *label)
{
    const struct glyph *g;

    frame();
    g = run_of(label);

    if (g)
    {
        click_at(g->x + 2, g->y + LH / 2);
    }

    return g != 0;
}

static int lit(const char *label)
{
    const struct glyph *g;

    frame();
    g = run_of(label);

    return g && back_at(g->x + 1, g->y + 1) == ed_colours[ED_C_MENU_HOT];
}

static const char *panel(const char *label)
{
    const struct glyph *g;

    frame();
    g = run_of(label);

    return g ? band(g->y + LH / 2) : "";
}

static void t_find(void)
{
    puts("Ctrl+F opens the panel, and typing in it finds as it goes");
    scratch("one two one Two ONE\nlast one");
    key('f', ED_MOD_CTRL);
    CHECK(ed_find_focus());
    type("on");
    CHECK(text_is("one two one Two ONE\nlast one") && sel_is(0u, 2u));
    type("e");
    CHECK(sel_is(0u, 3u));
    CHECK(strstr(panel("Find:"), "Find: one Next Previous Aa Word") && !strstr(panel("Find:"), "Wrapped"));

    puts("Enter and F3 go on to the next match, round the end; with shift they go back");
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(8u, 11u));
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(16u, 19u));
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(25u, 28u));
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(0u, 3u) && strstr(panel("Find:"), "Wrapped"));
    key(ED_KEY_ENTER, ED_MOD_SHIFT);
    CHECK(sel_is(25u, 28u));
    key(ED_KEY_F3, ED_MOD_SHIFT);
    CHECK(sel_is(16u, 19u));
    CHECK(choose(M_SEARCH, "Find Next") && sel_is(25u, 28u));

    puts("the case and whole-word buttons narrow what counts as a match, and take the keyboard");
    click(0u, 0u, 0u, 1u);
    CHECK(!ed_find_focus());
    CHECK(press("Aa") && ed_find_focus() && sel_is(0u, 3u));
    key(ED_KEY_F3, 0u);
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(25u, 28u));
    CHECK(lit("Aa") && !lit("Word"));
    press("Aa");
    key(ED_KEY_BACKSPACE, 0u);
    key(ED_KEY_BACKSPACE, 0u);
    click(0u, 0u, 0u, 1u);
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(0u, 1u));
    press("Word");
    key(ED_KEY_F3, 0u);
    CHECK(lit("Word") && strstr(panel("Find:"), "Not found"));
    press("Word");

    puts("a click in the text takes the keyboard back, and Escape shuts the panel");
    click(1u, 0u, 0u, 1u);
    CHECK(!ed_find_focus());
    type("X");
    CHECK(text_is("one two one Two ONE\nXlast one"));
    key('z', ED_MOD_CTRL);
    frame();
    click_at(run_of("Find:")->x + 80, run_of("Find:")->y + LH / 2);
    CHECK(ed_find_focus());
    key(ED_KEY_ESCAPE, 0u);
    CHECK(!ed_find_focus());
    frame();
    CHECK(!run_of("Find:"));

    puts("Ctrl+F on a selection looks for the selection");
    click(0u, 5u, 0u, 2u);
    CHECK(sel_is(4u, 7u));
    tool(TOOL_FIND);
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(12u, 15u));
    key(ED_KEY_ESCAPE, 0u);

    puts("Replace changes one match and moves on; Replace All is one edit that one undo takes back");
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key('h', ED_MOD_CTRL);
    key(ED_KEY_TAB, 0u);
    type("2");
    key(ED_KEY_ENTER, 0u);
    CHECK(text_is("one two one Two ONE\nlast one") && sel_is(4u, 7u));
    key(ED_KEY_ENTER, 0u);
    CHECK(text_is("one 2 one Two ONE\nlast one") && sel_is(10u, 13u));
    CHECK(press("Replace All") && text_is("one 2 one 2 ONE\nlast one"));
    CHECK(strstr(panel("Find:"), "1 replaced"));
    key(ED_KEY_TAB, 0u);
    keys(ED_KEY_BACKSPACE, 0u, 3u);
    type("one");
    key(ED_KEY_TAB, 0u);
    key(ED_KEY_BACKSPACE, 0u);
    type("[1]");
    CHECK(press("Replace All") && text_is("[1] 2 [1] 2 [1]\nlast [1]"));
    CHECK(strstr(panel("Find:"), "4 replaced"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("one 2 one 2 ONE\nlast one"));
    key(ED_KEY_ESCAPE, 0u);

    puts("Ctrl+G goes to a line, and takes only a number");
    key('g', ED_MOD_CTRL);
    type("x2y");
    CHECK(strstr(panel("Go to line:"), "Go to line: 2"));
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(16u, 16u) && !ed_find_focus());
    key('g', ED_MOD_CTRL);
    type("999");
    key(ED_KEY_ENTER, 0u);
    CHECK(sel_is(16u, 16u));
    only_one();
}

static void t_lines(void)
{
    puts("a line can be moved, duplicated and deleted, and its caret goes with it");
    scratch("1\n2\n3");
    click(1u, 0u, 0u, 1u);
    key(ED_KEY_DOWN, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("1\n3\n2") && sel_is(4u, 4u));
    key(ED_KEY_DOWN, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("1\n3\n2"));
    key(ED_KEY_UP, ED_MOD_CTRL | ED_MOD_SHIFT);
    key(ED_KEY_UP, ED_MOD_CTRL | ED_MOD_SHIFT);
    key(ED_KEY_UP, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("2\n1\n3") && sel_is(0u, 0u));
    key('d', ED_MOD_CTRL);
    CHECK(text_is("2\n2\n1\n3") && sel_is(2u, 2u));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("2\n1\n3"));
    key('l', ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("1\n3"));
    key(ED_KEY_END, ED_MOD_CTRL);
    CHECK(choose(M_EDIT, "Delete Line") && text_is("1"));
    CHECK(choose(M_EDIT, "Delete Line") && text_is(""));

    puts("several selected lines move as one");
    type("a\nb\nc\nd");
    click(1u, 0u, 0u, 1u);
    key(ED_KEY_DOWN, ED_MOD_SHIFT);
    key(ED_KEY_END, ED_MOD_SHIFT);
    CHECK(sel_is(2u, 5u));
    key(ED_KEY_UP, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("b\nc\na\nd") && sel_is(0u, 3u));
    key(ED_KEY_DOWN, ED_MOD_CTRL | ED_MOD_SHIFT);
    key(ED_KEY_DOWN, ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("a\nd\nb\nc") && sel_is(4u, 7u));

    puts("Tab pushes selected lines in, Shift+Tab pulls them back");
    key('a', ED_MOD_CTRL);
    key(ED_KEY_TAB, 0u);
    CHECK(text_is("\ta\n\td\n\tb\n\tc") && sel_is(0u, 11u));
    key(ED_KEY_TAB, ED_MOD_SHIFT);
    CHECK(text_is("a\nd\nb\nc") && sel_is(0u, 7u));
    key(ED_KEY_TAB, ED_MOD_SHIFT);
    CHECK(text_is("a\nd\nb\nc"));
    key('a', ED_MOD_CTRL);
    type("      six blanks");
    key(ED_KEY_TAB, ED_MOD_SHIFT);
    CHECK(text_is("  six blanks"));

    puts("case, and blanks at the ends of lines");
    key('a', ED_MOD_CTRL);
    type("Hello \xC3\xA9  \nb\t\n  c ");
    key('a', ED_MOD_CTRL);
    key('u', ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(text_is("HELLO \xC3\x89  \nB\t\n  C ") && sel_is(0u, 17u));
    key('u', ED_MOD_CTRL);
    CHECK(text_is("hello \xC3\xA9  \nb\t\n  c "));
    CHECK(choose(M_EDIT, "Trim Trailing Space") && text_is("hello \xC3\xA9\nb\n  c"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("hello \xC3\xA9  \nb\t\n  c "));
    only_one();
}

static void t_convert(void)
{
    static const uint8_t le[] = { 0xFF, 0xFE, 'x', 0, '\r', 0, '\n', 0, 'y', 0 };

    puts("line ends and encoding can be changed, and the next save writes them");
    scratch("x\ny");
    key('s', ED_MOD_CTRL);
    ed_picked(ED_PICK_SAVE, F("conv.txt"));
    CHECK(file_is(F("conv.txt"), "x\ny", 3u) && !sess_unsaved(ed_cur));
    CHECK(item_ticked(M_EDIT, "Line Ends: Unix (LF)") && item_ticked(M_ENCODING, "UTF-8"));
    CHECK(choose(M_EDIT, "Line Ends: Windows (CR LF)") && sess_unsaved(ed_cur));
    CHECK(strstr(status(), "length: 4 lines: 2") && strstr(status(), "Pos: 5 Windows (CR LF) UTF-8 INS"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("conv.txt"), "x\r\ny", 4u));
    CHECK(choose(M_ENCODING, "UTF-16 LE BOM") && sess_unsaved(ed_cur) && item_ticked(M_ENCODING, "UTF-16 LE BOM"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("conv.txt"), le, sizeof le));
    CHECK(choose(M_EDIT, "Line Ends: Macintosh (CR)") && choose(M_ENCODING, "ANSI"));
    key('s', ED_MOD_CTRL);
    CHECK(file_is(F("conv.txt"), "x\ry", 3u) && strstr(status(), "Macintosh (CR) ANSI INS"));

    puts("the language is the user's to choose, whatever the file is called");
    key('a', ED_MOD_CTRL);
    type("int x;");
    frame();
    CHECK(cell_ink(0u, 0u) == ed_colours[ED_C_TEXT] && item_ticked(M_LANGUAGE, "Normal Text"));
    CHECK(choose(M_LANGUAGE, "C"));
    frame();
    CHECK(cell_ink(0u, 0u) == syntax("type") && item_ticked(M_LANGUAGE, "C") && !strncmp(status(), "C source file", 13u));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("x\ny"));
    CHECK(choose(M_LANGUAGE, "Normal Text") && !strncmp(status(), "Normal text file", 16u));
    only_one();

    puts("a file's ending picks its language, and the menu has every language by its own name");
    spill(F("tool.py"), "def run():\n    return 'ok'\n", 28u);
    CHECK(!ed_open(F("tool.py")));
    frame();
    CHECK(cell_ink(0u, 0u) == syntax("keyword") && cell_ink(1u, 11u) == syntax("string"));
    CHECK(!strncmp(status(), "Python file", 11u) && item_ticked(M_LANGUAGE, "Python") && !item_ticked(M_LANGUAGE, "C"));
    CHECK(item_of(M_LANGUAGE, "C++") && item_of(M_LANGUAGE, "C#") && item_of(M_LANGUAGE, "PowerShell") && item_of(M_LANGUAGE, "Markdown"));
    CHECK(choose(M_LANGUAGE, "JSON") && !strncmp(status(), "JSON file", 9u));
    spill(F("page.HTML"), "<p class=\"x\">hi</p>\n", 20u);
    CHECK(!ed_open(F("page.HTML")));
    frame();
    CHECK(!strncmp(status(), "HTML file", 9u) && cell_ink(0u, 1u) == syntax("tag") && cell_ink(0u, 3u) == syntax("attribute"));
    only_one();
}

static void t_zoom(void)
{
    uint32_t i;

    puts("zoom makes the text bigger and leaves the window's furniture as it was");
    scratch("abc");
    keys('=', ED_MOD_CTRL, 10u);
    CHECK(ed_opt.zoom == 200);
    frame();
    CHECK(glyph_at(66 + 16 + 3, TEXT_Y + 5) && glyph_at(66 + 16 + 3, TEXT_Y + 5)->unit == 'b' && glyph_at(66 + 16 + 3, TEXT_Y + 5)->w == 16);
    CHECK(back_at(66 + 48 + 1, TEXT_Y + 20) == ed_colours[ED_C_CARET]);
    CHECK(!strcmp(bar(), "new 1 x new 2 * x +") && strstr(band(10), "File Edit Search View"));
    ed_mouse_down(66 + 16 + 3, TEXT_Y + 8, 0u, 1u);
    ed_mouse_up();
    CHECK(sel_is(1u, 1u));
    key(ED_KEY_END, 0u);
    CHECK(strstr(status(), "Ln: 1 Col: 4 Pos: 4"));
    tool(TOOL_ZOOM_IN);
    CHECK(ed_opt.zoom == 210);
    key('-', ED_MOD_CTRL);
    ed_wheel(1, ED_MOD_CTRL);
    CHECK(ed_opt.zoom == 190);
    ed_wheel(-1, ED_MOD_CTRL);
    CHECK(choose(M_VIEW, "Restore Default Zoom") && ed_opt.zoom == 100);

    for (i = 0u; i < 60u; i++)
    {
        key('-', ED_MOD_CTRL);
    }

    CHECK(ed_opt.zoom == 50);
    keys('=', ED_MOD_CTRL, 60u);
    CHECK(ed_opt.zoom == 400);
    key('0', ED_MOD_CTRL);
    frame();
    CHECK(cell_char(0u, 1u) == 'b');
    only_one();
}

static void t_status(void)
{
    puts("the status line counts bytes for length and Pos, cells for Col, and says what is selected");
    scratch("\xC3\xA9\xF0\x9F\x98\x80x\n\ty");
    CHECK(!strcmp(status(), "Normal text file length: 10 lines: 2 Ln: 2 Col: 6 Pos: 11 Unix (LF) UTF-8 INS"));
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key(ED_KEY_RIGHT, 0u);
    key(ED_KEY_RIGHT, 0u);
    CHECK(strstr(status(), "Ln: 1 Col: 4 Pos: 7 "));
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key(ED_KEY_DOWN, ED_MOD_SHIFT);
    CHECK(strstr(status(), "Ln: 2 Col: 1 Pos: 9 Sel: 5 | 2 Unix (LF)"));
    key(ED_KEY_BACKSPACE, 0u);
    CHECK(strstr(status(), "length: 2 lines: 1 Ln: 1 Col: 1 Pos: 1 Unix"));
    key('z', ED_MOD_CTRL);
    key(ED_KEY_END, ED_MOD_CTRL);
    CHECK(strstr(status(), "length: 10 lines: 2 Ln: 2 Col: 6 Pos: 11 Unix"));

    puts("Insert switches to typing over what is there, and the status line says so");
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key(ED_KEY_INSERT, 0u);
    CHECK(strstr(status(), "UTF-8 OVR") && item_ticked(M_EDIT, "Overwrite Mode"));
    type("E");
    type("m");
    type("X");
    CHECK(text_is("EmX\n\ty") && sel_is(3u, 3u));
    type("!");
    CHECK(text_is("EmX!\n\ty"));
    frame();
    CHECK(back_at(TEXT_X + 4 * FW + 3, TEXT_Y + LH - 1) == ed_colours[ED_C_CARET] && back_at(TEXT_X + 4 * FW + 1, TEXT_Y + 3) != ed_colours[ED_C_CARET]);
    key(ED_KEY_INSERT, 0u);
    CHECK(strstr(status(), "UTF-8 INS"));
    only_one();
}

static void t_reload(void)
{
    puts("Reload from Disk takes the file's text, and undo brings back what was there");
    spill(F("r.txt"), "disk one\n", 9u);
    CHECK(!ed_open(F("r.txt")));
    type("mine ");
    CHECK(sess_unsaved(ed_cur));
    spill(F("r.txt"), "disk two\r\n", 10u);
    CHECK(choose(M_FILE, "Reload from Disk") && text_is("disk two\n") && !sess_unsaved(ed_cur));
    CHECK(strstr(status(), "Windows (CR LF)"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("mine disk one\n") && sess_unsaved(ed_cur));

    puts("Save All writes every tab that has a file, and leaves the untitled ones to the session");
    spill(F("s.txt"), "s", 1u);
    CHECK(!ed_open(F("s.txt")));
    type("2");
    scratch("no file");
    key('s', ED_MOD_CTRL | ED_MOD_SHIFT);
    CHECK(file_is(F("r.txt"), "mine disk one\r\n", 15u) && file_is(F("s.txt"), "2s", 2u));
    CHECK(text_is("no file") && sess_unsaved(ed_cur));

    puts("Close All But This keeps one tab; a tab the user will not let go stops the rest");
    answer = ED_CLOSE_CANCEL;
    asks = 0u;
    ed_tab_show(0u);
    CHECK(choose(M_FILE, "Close All But This") && asks == 1u && sess_count() == 3u);
    answer = ED_CLOSE_DISCARD;
    ed_tab_show(0u);
    CHECK(choose(M_FILE, "Close All But This") && sess_count() == 1u && !strcmp(title, "r.txt"));
    only_one();
    CHECK(sess_count() == 1u && !strcmp(title, "new 1") && text_is(""));
}

static void t_clock(void)
{
    uint64_t now = 100000u;
    uint32_t puts_before;
    uint32_t i;

    puts("the session is flushed once the user goes quiet, not before");
    ed_tick(now);
    ed_tick(now += 2000u);
    ed_tick(now += 2000u);
    puts_before = store_puts;
    ed_tick(now += 20000u);
    CHECK(store_puts == puts_before);
    scratch("typed");
    ed_tick(now += 10u);
    ed_tick(now += 1000u);
    CHECK(store_puts == puts_before);
    ed_tick(now += 600u);
    CHECK(store_puts == puts_before + 2u);
    ed_tick(now += 5000u);
    CHECK(store_puts == puts_before + 2u);

    puts("moving the caret costs a manifest and no backup");
    key(ED_KEY_LEFT, 0u);
    ed_tick(now += 10u);
    ed_tick(now += 1600u);
    CHECK(store_puts == puts_before + 3u);

    puts("a user who never pauses is flushed anyway, within ten seconds");
    puts_before = store_puts;

    for (i = 0u; i < 20u; i++)
    {
        type("x");
        ed_tick(now += 500u);
    }

    CHECK(store_puts == puts_before);
    type("x");
    ed_tick(now += 600u);
    CHECK(store_puts == puts_before + 2u);

    puts("losing focus flushes at once");
    type("y");
    puts_before = store_puts;
    ed_focus_lost();
    CHECK(store_puts == puts_before + 2u);
    ed_tick(now += 5000u);
    CHECK(store_puts == puts_before + 2u);
    key('z', ED_MOD_CTRL);
    key('z', ED_MOD_CTRL);
    answer = ED_CLOSE_DISCARD;
    key('w', ED_MOD_CTRL);
}

static void t_scale(void)
{
    puts("a scale that is not a whole number: every band, gap and icon follows it, and clicks still land");
    scratch("abc");
    scale_now = 125;
    ed_resize(W, H, 125);
    frame();
    CHECK(ed_px(6) == 8 && ed_px(1) == 1 && ed_px(2) == 3 && ed_px(0) == 0);
    CHECK(ed_g.menu_h == 30 && ed_g.icon == 20 && ed_g.stroke == 2 && ed_g.tool_h == 35 && ed_g.bar_y == 65 && ed_g.text_y == 101 && ed_g.text_x == 54);
    CHECK(glyph_at(54 + 10 + 2, 101 + 4) && glyph_at(54 + 10 + 2, 101 + 4)->unit == 'b');
    ed_mouse_down(54 + 10 + 2, 101 + 4, 0u, 1u);
    ed_mouse_up();
    CHECK(sel_is(1u, 1u));
    click_at(8 + 14, 30 + 17);
    CHECK(sess_count() == 3u && text_is(""));
    key('w', ED_MOD_CTRL);
    scale_now = 150;
    ed_resize(W, H, 150);
    frame();
    CHECK(ed_g.menu_h == 36 && ed_g.icon == 24 && ed_g.stroke == 2 && ed_g.text_y == 120 && ed_g.text_x == 63 && ed_g.scroll_w == 18);
    CHECK(strstr(band(18), "File Edit Search View"));
    scale_now = 200;
    ed_resize(W, H, 200);
    frame();
    CHECK(ed_g.icon == 32 && ed_g.stroke == 2 && ed_g.menu_h == 48);
    scale_now = 100;
    ed_resize(W, H, 100);
    frame();
    CHECK(ed_g.menu_h == MENU_H && ed_g.icon == 16 && ed_g.stroke == 1 && ed_g.text_y == TEXT_Y && ed_g.text_x == TEXT_X);
    CHECK(cell_char(0u, 1u) == 'b');
    only_one();
}

static void t_wrap(void)
{
    char text[128];
    uint32_t i;

    puts("word wrap: a long line folds at a space, and the keys and the mouse go by what is seen");

    for (i = 0u; i < 100u; i++)
    {
        text[i] = i % 5u == 4u ? ' ' : 'a';
    }

    memcpy(text + 100u, "\nend", 5u);
    scratch(text);
    CHECK(choose(M_VIEW, "Word Wrap") && ed_opt.wrap && item_ticked(M_VIEW, "Word Wrap"));
    frame();
    CHECK(cell_char(0u, 69u) == ' ' && cell_char(0u, 70u) == 0u && cell_char(1u, 0u) == 'a' && cell_char(1u, 29u) == ' ' && cell_char(2u, 0u) == 'e');
    CHECK(!glyph_at(TEXT_X - 14, TEXT_Y + LH + 2) && glyph_at(TEXT_X - 14, TEXT_Y + LH * 2 + 2) && glyph_at(TEXT_X - 14, TEXT_Y + LH * 2 + 2)->unit == '2');
    key(ED_KEY_HOME, ED_MOD_CTRL);
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(70u, 70u));
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(101u, 101u));
    key(ED_KEY_UP, 0u);
    key(ED_KEY_END, 0u);
    CHECK(sel_is(100u, 100u));
    key(ED_KEY_UP, 0u);
    CHECK(sel_is(30u, 30u));
    key(ED_KEY_END, 0u);
    CHECK(sel_is(69u, 69u));
    key(ED_KEY_END, 0u);
    CHECK(sel_is(100u, 100u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(70u, 70u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(0u, 0u));
    click(1u, 3u, 0u, 1u);
    CHECK(sel_is(73u, 73u) && strstr(status(), "Ln: 1 Col: 74"));
    click(0u, 2u, 0u, 1u);
    click(1u, 2u, ED_MOD_SHIFT, 1u);
    frame();
    CHECK(cell_back(0u, 3u) == ed_colours[ED_C_SELECTION] && cell_back(1u, 1u) == ed_colours[ED_C_SELECTION] && cell_back(1u, 3u) != ed_colours[ED_C_SELECTION]);
    ed_wheel(1, 0u);
    CHECK(ed_now->top == 1u && ed_now->sub == 0u);
    ed_wheel(-1, 0u);
    CHECK(ed_now->top == 0u && ed_now->sub == 0u);
    ed_scroll_lines(1);
    frame();
    CHECK(ed_now->top == 0u && ed_now->sub == 1u && cell_char(0u, 0u) == 'a' && cell_char(1u, 0u) == 'e');
    key(ED_KEY_HOME, ED_MOD_CTRL);
    CHECK(ed_now->sub == 0u && ed_now->left == 0);

    puts("a narrower window folds again; with wrap off the line is one line");
    ed_resize(42 + 12 + 6 + 20 * FW, H, 100);
    frame();
    CHECK(cell_char(1u, 0u) == 'a' && cell_char(4u, 19u) == ' ' && cell_char(5u, 0u) == 'e');
    ed_resize(W, H, 100);
    key('z', ED_MOD_ALT);
    frame();
    CHECK(!ed_opt.wrap && cell_char(1u, 0u) == 'e' && cell_char(0u, 70u) == 'a');
    only_one();
}

static void t_csv(void)
{
    puts("a CSV file is painted by column, and a comma inside quotes stays in its cell");
    spill(F("t.csv"), "a,b,\"c,d\",e\n1,2,3,4\n", 20u);
    spill(F("t.tsv"), "x\ty\n", 4u);
    CHECK(!ed_open(F("t.csv")));
    frame();
    CHECK(cell_ink(0u, 0u) == ed_columns[0] && cell_ink(0u, 1u) == ed_colours[ED_C_DIM] && cell_ink(0u, 2u) == ed_columns[1]);
    CHECK(cell_ink(0u, 4u) == ed_columns[2] && cell_ink(0u, 6u) == ed_columns[2] && cell_ink(0u, 8u) == ed_columns[2] && cell_ink(0u, 10u) == ed_columns[3]);
    CHECK(cell_ink(1u, 0u) == ed_columns[0] && cell_ink(1u, 6u) == ed_columns[3]);
    CHECK(strstr(status(), "Comma-separated values file") && item_ticked(M_LANGUAGE, "CSV"));
    CHECK(choose(M_LANGUAGE, "Normal Text"));
    frame();
    CHECK(cell_ink(0u, 0u) == ed_colours[ED_C_TEXT] && cell_ink(0u, 1u) == ed_colours[ED_C_TEXT]);
    CHECK(choose(M_LANGUAGE, "TSV"));
    frame();
    CHECK(cell_ink(0u, 0u) == ed_columns[0] && cell_ink(0u, 2u) == ed_columns[0]);
    CHECK(!ed_open(F("t.tsv")));
    frame();
    CHECK(cell_ink(0u, 0u) == ed_columns[0] && cell_ink(0u, 4u) == ed_columns[1] && strstr(status(), "Tab-separated values file"));
    only_one();
}

static void t_fonts(void)
{
    const struct glyph *g;

    puts("the text and the interface each take a font from the list of installed ones");
    scratch("abc");
    CHECK(choose(M_VIEW, "Text Font...") && ed_list_shown());
    frame();
    CHECK(run_of("Text Font") && run_of("System default") && run_of("Alpha Mono") && run_of("Gamma Serif") && run_of("Type to filter"));
    type("BET");
    frame();
    CHECK(text_is("abc") && !run_of("Alpha Mono") && !run_of("System default") && run_of("Beta Sans") && run_of("BET"));
    key(ED_KEY_BACKSPACE, 0u);
    key(ED_KEY_ENTER, 0u);
    CHECK(!ed_list_shown() && !strcmp(font_used[ED_FONT_TEXT], "Beta Sans") && !strcmp(ed_opt.font_text, "Beta Sans") && !font_used[ED_FONT_UI][0]);
    CHECK(choose(M_VIEW, "Interface Font..."));
    frame();
    g = run_of("Gamma Serif");
    CHECK(run_of("Interface Font") && g);
    click_at(g->x + 4, g->y + 4);
    CHECK(!ed_list_shown() && !strcmp(font_used[ED_FONT_UI], "Gamma Serif") && !strcmp(ed_opt.font_ui, "Gamma Serif"));
    CHECK(choose(M_VIEW, "Text Font..."));
    key(ED_KEY_DOWN, 0u);
    key(ED_KEY_ESCAPE, 0u);
    CHECK(!ed_list_shown() && !strcmp(ed_opt.font_text, "Beta Sans"));
    CHECK(choose(M_VIEW, "Text Font..."));
    click_at(3, H - 40);
    CHECK(!ed_list_shown() && text_is("abc"));
    CHECK(choose(M_VIEW, "Text Font..."));
    key(ED_KEY_HOME, 0u);
    key(ED_KEY_ENTER, 0u);
    CHECK(!font_used[ED_FONT_TEXT][0] && !ed_opt.font_text[0]);
    CHECK(choose(M_VIEW, "Interface Font..."));
    key(ED_KEY_HOME, 0u);
    key(ED_KEY_ENTER, 0u);
    CHECK(!font_used[ED_FONT_UI][0] && !ed_opt.font_ui[0]);
    only_one();
}

static void pick_tongue(const char *label)
{
    const struct glyph *g;

    shut_menu();
    key('v', ED_MOD_ALT);
    keys(ED_KEY_UP, 0u, 1u);
    key(ED_KEY_ENTER, 0u);
    frame();
    g = run_of(label);
    CHECK(ed_list_shown() && g);

    if (g)
    {
        click_at(g->x + 4, g->y + 4);
    }
}

static void t_tongue(void)
{
    puts("the interface changes language from a file of words; what the file leaves out stays English");
    pick_tongue("Test");
    CHECK(!strcmp(ed_opt.tongue, "xx") && !strcmp(ed_word(ED_W_NEW), "Neu") && !strcmp(ed_word(ED_W_MENU_FILE), "Datei") && !strcmp(ed_word(ED_W_OPEN), "Open..."));
    frame();
    CHECK(strstr(band(10), "Datei Edit Search View") && strstr(status(), "Zeile: 1 Col: 1"));
    key('f', ED_MOD_ALT);
    frame();
    CHECK(run_of("Neu") && run_of("Open...") && !run_of("New"));
    shut_menu();
    CHECK(!ed_open(F("t.csv")) && strstr(status(), "Werte mit Komma") && !strstr(status(), "Comma-separated"));
    pick_tongue("zz");
    CHECK(!strcmp(ed_word(ED_W_NEW), "Nouveau") && !strcmp(ed_word(ED_W_MENU_FILE), "File") && strstr(status(), "Comma-separated values file"));
    key('w', ED_MOD_CTRL);
    pick_tongue("English");
    CHECK(!ed_opt.tongue[0] && !strcmp(ed_word(ED_W_NEW), "New"));
    frame();
    CHECK(strstr(band(10), "File Edit Search View"));
}

static void t_regex(void)
{
    puts("the find panel takes a regular expression when asked to");
    scratch("one two\nthree 22 two\n7");
    key('f', ED_MOD_CTRL);
    keys(ED_KEY_BACKSPACE, 0u, 40u);
    CHECK(press(".*") && ed_opt.find_regex && lit(".*"));
    type("t.o");
    key(ED_KEY_HOME, ED_MOD_CTRL);
    click(0u, 0u, 0u, 1u);
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(4u, 7u));
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(17u, 20u));
    key(ED_KEY_F3, ED_MOD_SHIFT);
    CHECK(sel_is(4u, 7u));
    key('h', ED_MOD_CTRL);
    keys(ED_KEY_BACKSPACE, 0u, 3u);
    type("(");
    CHECK(strstr(panel("Find:"), "Bad pattern"));
    key(ED_KEY_BACKSPACE, 0u);
    type("\\d+");
    click(0u, 0u, 0u, 1u);
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(14u, 16u));
    key(ED_KEY_F3, ED_MOD_SHIFT);
    CHECK(sel_is(21u, 22u) && strstr(panel("Find:"), "Wrapped"));
    click(0u, 0u, 0u, 1u);
    key('h', ED_MOD_CTRL);
    key(ED_KEY_TAB, 0u);
    keys(ED_KEY_BACKSPACE, 0u, 40u);
    type("N");
    key(ED_KEY_ENTER, 0u);
    CHECK(text_is("one two\nthree 22 two\n7") && sel_is(14u, 16u));
    key(ED_KEY_ENTER, 0u);
    CHECK(text_is("one two\nthree N two\n7") && sel_is(20u, 21u));
    CHECK(press("Replace All") && text_is("one two\nthree N two\nN") && strstr(panel("Find:"), "1 replaced"));
    key('z', ED_MOD_CTRL);
    key('z', ED_MOD_CTRL);
    CHECK(text_is("one two\nthree 22 two\n7"));
    CHECK(press("Word") && press("Replace All") && text_is("one two\nthree N two\nN"));
    CHECK(press("Word"));
    key('z', ED_MOD_CTRL);

    puts("the replacement can name what each group matched");
    click(0u, 0u, 0u, 1u);
    key('h', ED_MOD_CTRL);
    keys(ED_KEY_BACKSPACE, 0u, 40u);
    type("(\\w+) (t\\w+)");
    key(ED_KEY_TAB, 0u);
    keys(ED_KEY_BACKSPACE, 0u, 40u);
    type("$2-$1$9 $$ \\n$0");
    click(0u, 0u, 0u, 1u);
    key(ED_KEY_F3, 0u);
    CHECK(sel_is(0u, 7u));
    CHECK(press("Replace") && text_is("two-one $ \none two\nthree 22 two\n7"));
    key('z', ED_MOD_CTRL);
    CHECK(press("Replace All") && text_is("two-one $ \none two\nthree two-22 $ \n22 two\n7") && strstr(panel("Find:"), "2 replaced"));
    key('z', ED_MOD_CTRL);
    CHECK(text_is("one two\nthree 22 two\n7"));
    CHECK(press(".*") && !ed_opt.find_regex && !ed_opt.find_word);
    CHECK(press("Replace All") && text_is("one two\nthree 22 two\n7") && strstr(panel("Find:"), "Not found"));
    key(ED_KEY_ESCAPE, 0u);
    only_one();
}

static void t_long(void)
{
    static char text[10008];
    uint32_t i;

    puts("a line of ten thousand characters goes on below itself, and all of it can be reached");

    for (i = 0u; i < 10000u; i++)
    {
        text[i] = (char)('a' + i % 10u);
    }

    memcpy(text + 10000u, "\nend", 5u);
    scratch(text);
    CHECK(hl_len(ed_now->doc) == 10004u && hl_rows(ed_now->doc) == 2u && sel_is(10004u, 10004u));
    key(ED_KEY_HOME, ED_MOD_CTRL);
    frame();
    CHECK(cell_char(0u, 0u) == 'a' && cell_char(1u, 0u) == 'g' && cell_char(2u, 0u) == 'c' && cell_char(3u, 0u) == 'e');
    CHECK(!glyph_at(TEXT_X - 14, TEXT_Y + LH + 2) && glyph_at(TEXT_X - 14, TEXT_Y + LH * 3 + 2) && glyph_at(TEXT_X - 14, TEXT_Y + LH * 3 + 2)->unit == '2');
    CHECK(back_at(W - 6, TEXT_Y + 2) == ed_colours[ED_C_SCROLL]);
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(4096u, 4096u));
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(8192u, 8192u));
    key(ED_KEY_DOWN, 0u);
    CHECK(sel_is(10001u, 10001u));
    key(ED_KEY_UP, 0u);
    key(ED_KEY_END, 0u);
    CHECK(sel_is(10000u, 10000u) && strstr(status(), "Col: 10001"));
    key(ED_KEY_UP, 0u);
    CHECK(sel_is(5904u, 5904u));
    key(ED_KEY_END, 0u);
    CHECK(sel_is(8191u, 8191u));
    key(ED_KEY_END, 0u);
    CHECK(sel_is(10000u, 10000u));
    type("!");
    CHECK(hl_len(ed_now->doc) == 10005u && ed_unit(10000u) == '!');
    key('z', ED_MOD_CTRL);
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(8192u, 8192u));
    key(ED_KEY_HOME, 0u);
    CHECK(sel_is(0u, 0u) && ed_now->left == 0);
    click(2u, 5u, 0u, 1u);
    CHECK(sel_is(8197u, 8197u));

    puts("the scroll bar goes by how much of the text is above, not by how many lines");
    ed_scroll_lines(3);
    frame();
    CHECK(ed_now->top == 1u && cell_char(0u, 0u) == 'e');
    CHECK(back_at(W - 6, TEXT_Y + 2) != ed_colours[ED_C_SCROLL] && back_at(W - 6, H - STATUS_H - 4) == ed_colours[ED_C_SCROLL]);
    ed_mouse_down(W - 4, TEXT_Y + 125, 0u, 1u);
    ed_mouse_up();
    frame();
    CHECK(ed_now->top == 0u && ed_now->sub == 1024u && cell_char(0u, 0u) == 'g');
    key(ED_KEY_END, ED_MOD_CTRL);
    frame();
    CHECK(sel_is(10004u, 10004u) && back_at(W - 6, H - STATUS_H - 4) == ed_colours[ED_C_SCROLL]);
    only_one();
}

static void t_erase(void)
{
    puts("a port that keeps things where the user cannot see them offers to erase them");
    CHECK(choose(M_FILE, "Erase Everything Kept Here...") && erases == 1u && sess_count() == 1u);
}

static void t_settings(void)
{
    uint32_t puts_before;

    puts("what was chosen is written with the session, once, and only when it changed");
    ed_focus_lost();
    puts_before = store_puts;
    ed_focus_lost();
    CHECK(store_puts == puts_before);
    CHECK(choose(M_VIEW, "Text Font..."));
    type("beta");
    key(ED_KEY_ENTER, 0u);
    key('f', ED_MOD_CTRL);
    CHECK(press("Aa") && press(".*"));
    key(ED_KEY_ESCAPE, 0u);
    pick_tongue("Test");
    key('z', ED_MOD_ALT);
    key('=', ED_MOD_CTRL);
    ed_focus_lost();
    CHECK(store_puts > puts_before);
    puts_before = store_puts;
    ed_focus_lost();
    CHECK(store_puts == puts_before);
    key('z', ED_MOD_ALT);
    key('0', ED_MOD_CTRL);
}

static void t_leave(void)
{
    uint64_t now = 900000u;

    puts("four tabs are left behind, three of them unsaved, and the process just ends");
    spill(F("a.c"), "int a;\n", 7u);
    spill(F("b.c"), "int b;\n", 7u);
    spill(F("c.c"), "int c;\n", 7u);
    CHECK(sess_count() == 1u);
    CHECK(!ed_open(F("a.c")) && sess_count() == 1u);
    scratch("scratch\ntext");
    CHECK(!ed_open(F("b.c")));
    click(0u, 5u, 0u, 1u);
    type("2");
    CHECK(!ed_open(F("c.c")));
    click(0u, 5u, 0u, 1u);
    type("c");
    ed_tab_show(1u);
    click(0u, 3u, 0u, 1u);
    CHECK(sess_count() == 4u && sess_active() == 1u && text_is("scratch\ntext") && sel_is(3u, 3u));
    ed_tick(now);
    ed_tick(now + 2000u);
    key(ED_KEY_END, ED_MOD_CTRL);
    type(" LOST");
}

static void t_back(void)
{
    uint32_t pos;

    puts("the second run has the settings the first one left");
    CHECK(!strcmp(ed_opt.tongue, "xx") && !strcmp(ed_word(ED_W_NEW), "Neu") && !strcmp(ed_opt.font_text, "Beta Sans") && !strcmp(font_used[ED_FONT_TEXT], "Beta Sans"));
    CHECK(ed_opt.find_case && ed_opt.find_regex && !ed_opt.find_word && !ed_opt.wrap && ed_opt.zoom == 100 && ed_opt.tool && ed_opt.numbers);

    puts("the second run finds every tab, in order, and has loaded only the one showing");
    CHECK(sess_count() == 4u && sess_active() == 1u);
    CHECK(!strcmp(title, "new 1") && title_unsaved);
    CHECK(text_is("scratch\ntext") && sel_is(3u, 3u));

    for (pos = 0u; pos < 4u; pos++)
    {
        CHECK(ed_tab_of(sess_at(pos))->loaded == (pos == 1u));
    }

    puts("unsaved text comes back from its backup, not from its file, and is still unsaved");
    ed_tab_show(2u);
    CHECK(!strcmp(title, "b.c") && title_unsaved && text_is("int b2;\n") && !ed_now->conflict);
    frame();
    CHECK(cell_ink(0u, 0u) == syntax("type") && strstr(status(), "length:"));
    CHECK(file_is(F("b.c"), "int b;\n", 7u));

    puts("a file that changed under unsaved text is flagged, and neither version is thrown away");
    ed_tab_show(3u);
    CHECK(text_is("int cc;\n") && ed_now->conflict && file_is(F("c.c"), "changed elsewhere\n", 18u));
    frame();
    CHECK(strstr(status(), "file changed on disk"));
    key('s', ED_MOD_CTRL);
    frame();
    CHECK(!ed_now->conflict && !strstr(status(), "file changed on disk") && file_is(F("c.c"), "int cc;\n", 8u));

    puts("a clean tab comes back from its file");
    ed_tab_show(0u);
    CHECK(!strcmp(title, "a.c") && !title_unsaved && text_is("edited outside\n"));
}

int main(int argc, char **argv)
{
    char path[512];
    char name[64];
    int back = argc > 2 && !strcmp(argv[2], "back");

    base = argc > 1 ? argv[1] : "ed_test.tmp";

    store_dir(base);
    snprintf(path, sizeof path, "%s/files", base);
    store_dir(path);
    snprintf(path, sizeof path, "%s/session", base);
    store_dir(path);

    if (back)
    {
        spill(F("c.c"), "changed elsewhere\n", 18u);
        spill(F("a.c"), "edited outside\n", 15u);
        CHECK(!ed_init(W, H, 100));
        t_back();
        printf("%d checks, %d failed\n", checks, fails);

        return fails ? 1 : 0;
    }

    while (!sess_list(1, name, sizeof name))
    {
        sess_del(name);
    }

    CHECK(!ed_init(W, H, 100));
    t_start();
    t_typing();
    t_moving();
    t_deleting();
    t_undo();
    t_clipboard();
    t_unicode();
    t_mouse();
    t_scrolling();
    t_files();
    t_tabs();
    t_names();
    t_menus();
    t_toolbar();
    t_find();
    t_lines();
    t_convert();
    t_zoom();
    t_status();
    t_reload();
    t_clock();
    t_scale();
    t_wrap();
    t_csv();
    t_fonts();
    t_tongue();
    t_regex();
    t_long();
    t_erase();
    t_settings();
    t_leave();
    printf("%d checks, %d failed\n", checks, fails);

    return fails ? 1 : 0;
}
