#include "ed_int.h"

#define PART_PAD ed_px(10)
#define SAY 96u

static uint32_t say(uint16_t *out, uint32_t n, const char *utf8)
{
    uint32_t len = 0u;

    while (utf8[len])
    {
        len++;
    }

    return n + ed_utf8_units(utf8, len, out + n, n < SAY - 12u ? SAY - 12u - n : 0u);
}

static uint32_t word(uint16_t *out, uint32_t n, uint32_t id)
{
    n = say(out, n, ed_word(id));

    return say(out, n, " ");
}

static uint32_t count(uint16_t *out, uint32_t n, uint32_t v)
{
    uint16_t back[10];
    uint32_t k = 0u;

    do
    {
        back[k++] = (uint16_t)('0' + v % 10u);
        v /= 10u;
    } while (v);

    while (k)
    {
        out[n++] = back[--k];
    }

    return n;
}

static void part(int32_t *right, const uint16_t *text, uint32_t n)
{
    int32_t w = ed_text_width(text, n) + PART_PAD * 2;
    int32_t y = ed_g.status_y;

    *right -= w;
    ed_draw_rect(*right, y + ed_px(3), 1, ed_g.status_h - ed_px(6), ed_colours[ED_C_BORDER]);
    ed_draw_text(*right + PART_PAD, y + ed_px(4), text, n, ed_colours[ED_C_STATUS_TEXT]);
}

static uint32_t where(uint16_t *text, uint32_t crlf)
{
    struct ed_tab *t = ed_now;
    uint32_t row = 0u;
    uint32_t col = 0u;
    uint32_t row2 = 0u;
    uint32_t n;

    hl_row_col(t->doc, t->caret, &row, &col);
    n = word(text, 0u, ED_W_S_LN);
    n = count(text, n, row + 1u);
    n = say(text, n, "   ");
    n = word(text, n, ED_W_S_COL);
    n = count(text, n, ed_col_of(t->caret) + 1u);
    n = say(text, n, "   ");
    n = word(text, n, ED_W_S_POS);
    n = count(text, n, ed_bytes_before(t->caret) + row * crlf + 1u);

    if (t->caret == t->anchor)
    {
        return n;
    }

    hl_row_col(t->doc, ed_sel_from(), &row, &col);
    hl_row_col(t->doc, ed_sel_to(), &row2, &col);
    n = say(text, n, "   ");
    n = word(text, n, ED_W_S_SEL);
    n = count(text, n, ed_sel_to() - ed_sel_from());
    n = say(text, n, " | ");

    return count(text, n, row2 - row + 1u);
}

static uint32_t trouble(const struct ed_tab *t, uint16_t *text)
{
    uint32_t id = t->conflict ? ED_W_S_CONFLICT : t->save_failed ? ED_W_S_SAVE_FAILED : t->missing ? ED_W_S_MISSING : ED_NONE;

    return id == ED_NONE ? 0u : say(text, say(text, 0u, ed_word(id)), "   ");
}

void ed_status_draw(void)
{
    static const char *const encodings[] = { "UTF-8", "UTF-8-BOM", "UTF-16 LE BOM", "UTF-16 BE BOM", "ANSI" };
    static const uint32_t eols[] = { ED_W_S_EOL_LF, ED_W_S_EOL_CRLF, ED_W_S_EOL_CR };
    struct ed_tab *t = ed_now;
    struct sess_tab v;
    uint16_t text[SAY];
    uint16_t warn[SAY];
    const char *kind;
    uint32_t crlf;
    uint32_t warn_n;
    uint32_t n;
    int32_t right = ed_w;
    int32_t x = ED_PAD;
    int32_t y = ed_g.status_y;

    if (!ed_g.status_h)
    {
        return;
    }

    ed_text_font(ED_FONT_UI, 100);
    ed_draw_clip(0, y, ed_w, ed_g.status_h);
    ed_draw_rect(0, y, ed_w, ed_g.status_h, ed_colours[ED_C_STATUS_BACK]);

    if (!t || !t->loaded || sess_tab_get(ed_cur, &v))
    {
        return;
    }

    crlf = v.eol == ED_EOL_CRLF ? 1u : 0u;
    kind = t->ext == ED_NONE ? ed_word(ED_W_S_NORMAL) : ed_lang_name(t->ext);
    warn_n = trouble(t, warn);
    part(&right, text, say(text, 0u, ed_word(ed_opt.overwrite ? ED_W_S_OVR : ED_W_S_INS)));
    part(&right, text, say(text, 0u, encodings[v.encoding < 5u ? v.encoding : 0u]));
    part(&right, text, say(text, 0u, ed_word(eols[v.eol < 3u ? v.eol : 0u])));
    n = where(text, crlf);
    ed_text_font(ED_FONT_UI, 100);
    part(&right, text, n);
    n = word(text, 0u, ED_W_S_LENGTH);
    n = count(text, n, t->bytes + (hl_rows(t->doc) - 1u) * crlf);
    n = say(text, n, "   ");
    n = word(text, n, ED_W_S_LINES);
    n = count(text, n, hl_rows(t->doc));

    if (right - ed_text_width(text, n) - PART_PAD * 2 >= ED_PAD * 2 + ed_text_width(warn, warn_n) + ed_say_width(kind))
    {
        part(&right, text, n);
    }

    ed_draw_clip(0, y, right > 0 ? right : 0, ed_g.status_h);
    ed_draw_text(x, y + ed_px(4), warn, warn_n, ed_colours[ED_C_STATUS_WARN]);
    x += ed_text_width(warn, warn_n);
    ed_say(x, y + ed_px(4), kind, ed_colours[ED_C_STATUS_TEXT]);
}
