#include "ed_int.h"

#define ZOOM_MIN 50
#define ZOOM_MAX 400
#define ZOOM_STEP 10
#define C ED_MOD_CTRL
#define S ED_MOD_SHIFT
#define A ED_MOD_ALT

struct ed_opts ed_opt = { 1, 1, 1, 0, 0, 100, 0, 0, 0, 0, "", "", "" };

struct key {
    uint32_t key, mods, cmd, arg;
    const char *label;
};

static const struct key keys[] = {
    { 'n', C, ED_CMD_NEW, 0u, "Ctrl+N" },
    { 'o', C, ED_CMD_OPEN, 0u, "Ctrl+O" },
    { 's', C, ED_CMD_SAVE, 0u, "Ctrl+S" },
    { 's', C | A, ED_CMD_SAVE_AS, 0u, "Ctrl+Alt+S" },
    { 's', C | S, ED_CMD_SAVE_ALL, 0u, "Ctrl+Shift+S" },
    { 'w', C, ED_CMD_CLOSE, 0u, "Ctrl+W" },
    { 'z', C, ED_CMD_UNDO, 0u, "Ctrl+Z" },
    { 'y', C, ED_CMD_REDO, 0u, "Ctrl+Y" },
    { 'z', C | S, ED_CMD_REDO, 0u, 0 },
    { 'x', C, ED_CMD_CUT, 0u, "Ctrl+X" },
    { 'c', C, ED_CMD_COPY, 0u, "Ctrl+C" },
    { 'v', C, ED_CMD_PASTE, 0u, "Ctrl+V" },
    { 'a', C, ED_CMD_SELECT_ALL, 0u, "Ctrl+A" },
    { 'd', C, ED_CMD_DUP_LINE, 0u, "Ctrl+D" },
    { 'l', C | S, ED_CMD_DEL_LINE, 0u, "Ctrl+Shift+L" },
    { ED_KEY_UP, C | S, ED_CMD_LINE_UP, 0u, "Ctrl+Shift+Up" },
    { ED_KEY_DOWN, C | S, ED_CMD_LINE_DOWN, 0u, "Ctrl+Shift+Down" },
    { ED_KEY_TAB, S, ED_CMD_UNINDENT, 0u, "Shift+Tab" },
    { 'u', C | S, ED_CMD_UPPER, 0u, "Ctrl+Shift+U" },
    { 'u', C, ED_CMD_LOWER, 0u, "Ctrl+U" },
    { ED_KEY_INSERT, 0u, ED_CMD_OVERWRITE, 0u, "Ins" },
    { 'f', C, ED_CMD_FIND, 0u, "Ctrl+F" },
    { ED_KEY_F3, 0u, ED_CMD_FIND_NEXT, 0u, "F3" },
    { ED_KEY_F3, S, ED_CMD_FIND_PREV, 0u, "Shift+F3" },
    { 'h', C, ED_CMD_REPLACE, 0u, "Ctrl+H" },
    { 'g', C, ED_CMD_GOTO, 0u, "Ctrl+G" },
    { '=', C, ED_CMD_ZOOM_IN, 0u, "Ctrl++" },
    { '+', C, ED_CMD_ZOOM_IN, 0u, 0 },
    { '=', C | S, ED_CMD_ZOOM_IN, 0u, 0 },
    { '-', C, ED_CMD_ZOOM_OUT, 0u, "Ctrl+-" },
    { '0', C, ED_CMD_ZOOM_RESET, 0u, "Ctrl+0" },
    { 'z', A, ED_CMD_WRAP, 0u, "Alt+Z" },
    { ED_KEY_TAB, C, ED_CMD_TAB_NEXT, 0u, "Ctrl+Tab" },
    { ED_KEY_TAB, C | S, ED_CMD_TAB_PREV, 0u, "Ctrl+Shift+Tab" },
    { ED_KEY_PAGE_DOWN, C, ED_CMD_TAB_NEXT, 0u, 0 },
    { ED_KEY_PAGE_UP, C, ED_CMD_TAB_PREV, 0u, 0 },
};

int ed_key_cmd(uint32_t key, uint32_t mods, uint32_t *cmd, uint32_t *arg)
{
    uint32_t i;

    for (i = 0u; i < sizeof keys / sizeof keys[0]; i++)
    {
        if (keys[i].key == key && keys[i].mods == (mods & (C | S | A)))
        {
            *cmd = keys[i].cmd;
            *arg = keys[i].arg;

            return 1;
        }
    }

    return 0;
}

const char *ed_cmd_key(uint32_t cmd, uint32_t arg)
{
    uint32_t i;

    for (i = 0u; i < sizeof keys / sizeof keys[0]; i++)
    {
        if (keys[i].cmd == cmd && keys[i].arg == arg && keys[i].label)
        {
            return keys[i].label;
        }
    }

    return 0;
}

static void close_all(uint32_t keep)
{
    uint32_t pos = sess_count();

    while (pos--)
    {
        if (sess_at(pos) == keep)
        {
            continue;
        }

        ed_tab_show(pos);

        if (!ed_tab_close())
        {
            return;
        }
    }

    for (pos = 0u; pos < sess_count(); pos++)
    {
        if (sess_at(pos) == keep)
        {
            ed_tab_show(pos);
        }
    }
}

static void rewrite(uint32_t cmd, uint32_t arg)
{
    struct sess_tab v;

    if (sess_tab_get(ed_cur, &v))
    {
        return;
    }

    if (cmd == ED_CMD_EOL)
    {
        v.eol = arg;
    }

    else
    {
        v.encoding = arg;
    }

    sess_tab_set(ed_cur, &v);
    sess_edited(ed_cur);
}

static int same_language(uint32_t a, uint32_t b)
{
    uint32_t i;

    if (a == ED_NONE || b == ED_NONE)
    {
        return a == b;
    }

    for (i = 0u; ed_exts[a].tag[i] && ed_exts[a].tag[i] == ed_exts[b].tag[i]; i++)
    {
    }

    return ed_exts[a].tag[i] == ed_exts[b].tag[i];
}

static void zoom(int32_t to)
{
    to = to < ZOOM_MIN ? ZOOM_MIN : to;
    ed_opt.zoom = to > ZOOM_MAX ? ZOOM_MAX : to;

    if (ed_now && ed_now->loaded)
    {
        ed_reveal();
    }
}

static void rewrap(void)
{
    ed_opt.wrap = !ed_opt.wrap;

    if (ed_now && ed_now->loaded)
    {
        ed_now->sub = 0u;
        ed_reveal();
    }
}

static void show_next(int back)
{
    uint32_t n = sess_count();

    ed_tab_show((sess_active() + (back ? n - 1u : 1u)) % n);
}

int ed_can(uint32_t cmd, uint32_t arg)
{
    const struct ed_tab *t = ed_now;
    int loaded = t && t->loaded;

    (void)arg;

    switch (cmd)
    {
        case ED_CMD_NEW:
        case ED_CMD_OPEN:
        case ED_CMD_EXIT:
        case ED_CMD_CLOSE:
        case ED_CMD_CLOSE_ALL:
        case ED_CMD_TOOLBAR:
        case ED_CMD_STATUS:
        case ED_CMD_NUMBERS:
        case ED_CMD_SPACES:
        case ED_CMD_WRAP:
        case ED_CMD_FONT_TEXT:
        case ED_CMD_FONT_UI:
        case ED_CMD_TONGUE:
        case ED_CMD_ZOOM_IN:
        case ED_CMD_ZOOM_OUT:
        case ED_CMD_ZOOM_RESET:
        case ED_CMD_OVERWRITE:
        case ED_CMD_TAB:
            return 1;

        case ED_CMD_ERASE:
            return ed_erase_offered != 0;

        case ED_CMD_CLOSE_OTHERS:
        case ED_CMD_TAB_NEXT:
        case ED_CMD_TAB_PREV:
            return sess_count() > 1u;

        case ED_CMD_RELOAD:
            return loaded && sess_tab_path(ed_cur) != 0;

        case ED_CMD_UNDO:
            return loaded && t->undo.pos > 0u;

        case ED_CMD_REDO:
            return loaded && t->undo.pos < t->undo.n;

        case ED_CMD_CUT:
        case ED_CMD_COPY:
        case ED_CMD_DELETE:
        case ED_CMD_UPPER:
        case ED_CMD_LOWER:
            return loaded && t->caret != t->anchor;

        case ED_CMD_FIND_NEXT:
        case ED_CMD_FIND_PREV:
            return loaded && ed_find_ready();

        default:
            return loaded;
    }
}

int ed_on(uint32_t cmd, uint32_t arg)
{
    struct sess_tab v;

    switch (cmd)
    {
        case ED_CMD_TOOLBAR:
            return ed_opt.tool;

        case ED_CMD_STATUS:
            return ed_opt.status;

        case ED_CMD_NUMBERS:
            return ed_opt.numbers;

        case ED_CMD_SPACES:
            return ed_opt.spaces;

        case ED_CMD_OVERWRITE:
            return ed_opt.overwrite;

        case ED_CMD_WRAP:
            return ed_opt.wrap;

        case ED_CMD_EOL:
            return !sess_tab_get(ed_cur, &v) && v.eol == arg;

        case ED_CMD_ENCODING:
            return !sess_tab_get(ed_cur, &v) && v.encoding == arg;

        case ED_CMD_LANGUAGE:
            return ed_now && same_language(ed_now->ext, arg);

        case ED_CMD_TAB:
            return sess_active() == arg;

        default:
            return 0;
    }
}

void ed_run(uint32_t cmd, uint32_t arg)
{
    if (!ed_can(cmd, arg))
    {
        return;
    }

    switch (cmd)
    {
        case ED_CMD_NEW: ed_tab_new(sess_active() + 1u, 0); break;
        case ED_CMD_OPEN: ed_pick(ED_PICK_OPEN); break;
        case ED_CMD_RELOAD: ed_tab_reload(); break;
        case ED_CMD_SAVE: ed_tab_save(0); break;
        case ED_CMD_SAVE_AS: ed_tab_save(1); break;
        case ED_CMD_SAVE_ALL: ed_tab_save_all(); break;
        case ED_CMD_CLOSE: ed_tab_close(); break;
        case ED_CMD_CLOSE_ALL: close_all(ED_NONE); break;
        case ED_CMD_CLOSE_OTHERS: close_all(ed_cur); break;
        case ED_CMD_EXIT: ed_exit(); break;
        case ED_CMD_ERASE: ed_erase(); break;
        case ED_CMD_EOL: rewrite(cmd, arg); break;
        case ED_CMD_ENCODING: rewrite(cmd, arg); break;
        case ED_CMD_OVERWRITE: ed_opt.overwrite = !ed_opt.overwrite; break;
        case ED_CMD_FIND: ed_find_open(cmd); break;
        case ED_CMD_REPLACE: ed_find_open(cmd); break;
        case ED_CMD_GOTO: ed_find_open(cmd); break;
        case ED_CMD_FIND_NEXT: ed_find_next(0); break;
        case ED_CMD_FIND_PREV: ed_find_next(1); break;
        case ED_CMD_TOOLBAR: ed_opt.tool = !ed_opt.tool; break;
        case ED_CMD_STATUS: ed_opt.status = !ed_opt.status; break;
        case ED_CMD_NUMBERS: ed_opt.numbers = !ed_opt.numbers; break;
        case ED_CMD_SPACES: ed_opt.spaces = !ed_opt.spaces; break;
        case ED_CMD_WRAP: rewrap(); break;
        case ED_CMD_FONT_TEXT: ed_list_open(cmd); break;
        case ED_CMD_FONT_UI: ed_list_open(cmd); break;
        case ED_CMD_TONGUE: ed_list_open(cmd); break;
        case ED_CMD_ZOOM_IN: zoom(ed_opt.zoom + ZOOM_STEP); break;
        case ED_CMD_ZOOM_OUT: zoom(ed_opt.zoom - ZOOM_STEP); break;
        case ED_CMD_ZOOM_RESET: zoom(100); break;
        case ED_CMD_LANGUAGE: ed_tab_lang(arg); break;
        case ED_CMD_TAB_NEXT: show_next(0); break;
        case ED_CMD_TAB_PREV: show_next(1); break;
        case ED_CMD_TAB: ed_tab_show(arg); break;
        default: ed_edit_run(cmd); break;
    }

    ed_touch();
}
