#include "ed_int.h"

#define QUIET_MS 1500u
#define LATEST_MS 10000u
#define PARSE_MS 300u
#define WHEEL_ROWS 3

int ed_close_after_save;

static int ready;
static int stale;
static int stirred;
static int drag;
static int owed;
static uint64_t owed_since;
static uint64_t stirred_at;

void ed_touch(void)
{
    stale = 1;
    stirred = 1;
}

static void settle(void)
{
    ed_tab_sync();
    ed_tab_title();
    ed_touch();
}

static void flush(void)
{
    ed_tab_sync();
    ed_cfg_save();

    if (!sess_flush())
    {
        owed = 0;
    }
}


uint32_t sess_buf_len(uint32_t tab)
{
    const struct ed_tab *t = ed_tab_of(tab);

    return t && t->loaded ? hl_len(t->doc) : 0u;
}

const uint16_t *sess_buf_run(uint32_t tab, uint32_t off, uint32_t *n)
{
    const struct ed_tab *t = ed_tab_of(tab);

    if (!t || !t->loaded)
    {
        *n = 0u;

        return 0;
    }

    return hl_text(t->doc, off, n);
}

static void view_down(int32_t x, int32_t y, uint32_t mods, uint32_t clicks)
{
    uint32_t pos = 0u;
    uint32_t what = ed_hit(x, y, &pos);
    uint32_t off;

    if (what == ED_HIT_TAB || what == ED_HIT_TAB_CLOSE)
    {
        ed_tab_show(pos);
    }

    if (what == ED_HIT_TAB_CLOSE)
    {
        ed_tab_close();
    }

    if (what == ED_HIT_TAB_NEW)
    {
        ed_tab_new(sess_count(), 0);
    }

    if (what == ED_HIT_SCROLL && ed_now->loaded)
    {
        ed_scroll_to(y);
        drag = 2;
    }

    if (what != ED_HIT_TEXT || !ed_now->loaded)
    {
        return;
    }

    ed_find_blur();
    off = ed_off_at(x, y);
    ed_now->caret = off;
    ed_now->want_x = -1;
    ed_now->undo.typing = 0;
    drag = 1;

    if (clicks == 2u)
    {
        ed_select_word(off);
    }

    else if (clicks >= 3u)
    {
        ed_select_row(off);
    }

    else if (!(mods & ED_MOD_SHIFT))
    {
        ed_now->anchor = off;
    }
}

int ed_init(int32_t w, int32_t h, int32_t scale)
{
    ed_w = w;
    ed_h = h;
    ed_scale = scale < 50 ? 50 : scale > 800 ? 800 : scale;

    if (sess_load() < 0)
    {
        return -1;
    }

    ed_cfg_load();

    if (ed_tabs_adopt())
    {
        return -1;
    }

    ready = 1;

    if (!sess_count() && ed_tab_new(0u, 0))
    {
        ready = 0;

        return -1;
    }

    ed_tab_show(sess_active());

    return 0;
}

void ed_resize(int32_t w, int32_t h, int32_t scale)
{
    ed_w = w;
    ed_h = h;
    ed_scale = scale < 50 ? 50 : scale > 800 ? 800 : scale;
    stale = 1;
}

void ed_key(uint32_t key, uint32_t mods)
{
    uint32_t cmd;
    uint32_t arg;

    if (!ready)
    {
        return;
    }

    if (ed_list_shown())
    {
        ed_list_key(key);
        settle();
        return;
    }

    if (ed_menu_key(key, mods) || (ed_find_focus() && ed_find_key(key, mods)))
    {
        settle();
        return;
    }

    if (ed_key_cmd(key, mods, &cmd, &arg))
    {
        ed_run(cmd, arg);
    }

    else if (key == ED_KEY_ESCAPE)
    {
        ed_find_close();
    }

    else if (ed_now->loaded && !ed_find_focus())
    {
        ed_edit_key(key, mods);
    }

    settle();
}

void ed_text(const char *utf8, uint32_t n)
{
    uint16_t *text;
    uint32_t len;
    uint32_t unused = 0u;

    if (!ready || (ed_menu_open() && !ed_list_shown()) || !ed_now->loaded || ed_decode((const uint8_t *)utf8, n, 0, &text, &len, &unused, &unused))
    {
        return;
    }

    if (ed_list_shown())
    {
        ed_list_text(text, len);
    }

    else if (ed_find_focus())
    {
        ed_find_text(text, len);
    }

    else
    {
        ed_edit_text(text, len);
    }

    ed_mem_free(text);
    settle();
}

void ed_mouse_down(int32_t x, int32_t y, uint32_t mods, uint32_t clicks)
{
    if (!ready)
    {
        return;
    }

    ed_measure();

    if (ed_list_shown())
    {
        ed_list_down(x, y);
    }

    else if (!ed_menu_down(x, y) && !ed_tool_down(x, y) && !ed_find_down(x, y))
    {
        view_down(x, y, mods, clicks);
    }

    settle();
}

void ed_mouse_move(int32_t x, int32_t y)
{
    if (!ready)
    {
        return;
    }

    ed_measure();

    if (ed_list_shown())
    {
        ed_list_move(x, y);
        return;
    }

    if (ed_menu_move(x, y))
    {
        return;
    }

    ed_tool_move(x, y);

    if (!drag || !ed_now->loaded)
    {
        return;
    }

    if (drag == 2)
    {
        ed_scroll_to(y);
    }

    else
    {
        ed_now->caret = ed_off_at(x, y);
        ed_reveal();
    }

    settle();
}

void ed_mouse_up(void)
{
    drag = 0;
}

void ed_wheel(int32_t rows, uint32_t mods)
{
    if (ready && ed_list_shown())
    {
        ed_list_wheel(rows);
        return;
    }

    if (!ready || ed_menu_open() || !ed_now->loaded)
    {
        return;
    }

    if (mods & ED_MOD_CTRL)
    {
        ed_run(rows < 0 ? ED_CMD_ZOOM_IN : ED_CMD_ZOOM_OUT, 0u);
        settle();
        return;
    }

    ed_scroll_lines(rows * WHEEL_ROWS);
    settle();
}

void ed_tick(uint64_t now_ms)
{
    if (!ready)
    {
        return;
    }

    if (stirred)
    {
        stirred = 0;
        stirred_at = now_ms;
        owed_since = owed ? owed_since : now_ms;
        owed = 1;
    }

    if (now_ms - stirred_at >= PARSE_MS && hl_settle())
    {
        stale = 1;
    }

    if (owed && (now_ms - stirred_at >= QUIET_MS || now_ms - owed_since >= LATEST_MS))
    {
        owed_since = now_ms;
        stirred_at = now_ms;
        flush();
    }
}

void ed_focus_lost(void)
{
    if (!ready)
    {
        return;
    }

    drag = 0;
    flush();
}

int ed_open(const char *path)
{
    uint32_t pos;
    uint32_t i;

    if (!ready || !path || !path[0])
    {
        return -1;
    }

    for (pos = 0u; pos < sess_count(); pos++)
    {
        for (i = 0u; sess_tab_path(sess_at(pos)) && path[i] && path[i] == sess_tab_path(sess_at(pos))[i]; i++)
        {
        }

        if (sess_tab_path(sess_at(pos)) && !path[i] && !sess_tab_path(sess_at(pos))[i])
        {
            ed_tab_show(pos);
            settle();

            return 0;
        }
    }

    pos = sess_active();
    i = ed_now->loaded && !sess_tab_path(ed_cur) && !sess_unsaved(ed_cur) && !hl_len(ed_now->doc);

    if (ed_tab_new(pos + 1u, path))
    {
        return -1;
    }

    if (i)
    {
        ed_tab_show(pos);
        ed_tab_close();
        ed_tab_show(pos);
    }

    settle();

    return 0;
}

void ed_picked(uint32_t what, const char *path)
{
    if (!ready)
    {
        return;
    }

    if (what == ED_PICK_OPEN && path)
    {
        ed_open(path);
    }

    if (what == ED_PICK_SAVE && path)
    {
        ed_tab_saved_as(path);
    }

    ed_close_after_save = 0;
    settle();
}

int ed_stale(void)
{
    return ready && stale;
}

void ed_draw(void)
{
    if (!ready)
    {
        return;
    }

    ed_measure();
    ed_draw_clip(0, 0, ed_w, ed_h);
    ed_draw_rect(0, 0, ed_w, ed_h, ed_colours[ED_C_BACK]);
    ed_view_draw();
    ed_find_draw();
    ed_status_draw();
    ed_tool_draw();
    ed_menu_draw();
    ed_menu_draw_open();
    ed_list_draw();
    ed_draw_clip(0, 0, ed_w, ed_h);
    stale = 0;
}
