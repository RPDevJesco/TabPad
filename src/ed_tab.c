#include "ed_int.h"

#define NAME_BYTES 256u

static struct ed_tab *tabs;
static uint32_t tab_max;

uint32_t ed_cur = ED_NONE;
struct ed_tab *ed_now;

static uint32_t told_tab = ED_NONE;
static int told_unsaved;

static int room(uint32_t tab)
{
    struct ed_tab *grown;
    uint32_t max = tab_max ? tab_max : 16u;
    uint32_t i;

    if (tab < tab_max)
    {
        return 1;
    }

    while (max <= tab)
    {
        max *= 2u;
    }

    grown = ed_mem_alloc((size_t)max * sizeof *grown);

    if (!grown)
    {
        return 0;
    }

    for (i = 0u; i < tab_max; i++)
    {
        grown[i] = tabs[i];
    }

    ed_mem_free(tabs);
    tabs = grown;
    tab_max = max;

    ed_now = ed_cur == ED_NONE ? 0 : &tabs[ed_cur];

    return 1;
}

static void blank(struct ed_tab *t)
{
    static const struct ed_tab none;

    *t = none;
    t->lang = HL_PLAIN;
    t->ext = ED_NONE;
    t->want_x = -1;
}

static uint32_t length(const char *s)
{
    uint32_t n = 0u;

    while (s && s[n])
    {
        n++;
    }

    return n;
}

static char lower(char c)
{
    return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

static const char *leaf(const char *path)
{
    const char *p = path;

    for (; *path; path++)
    {
        if (*path == '/' || *path == '\\')
        {
            p = path + 1;
        }
    }

    return p;
}

static uint32_t ext_of(const char *path)
{
    uint32_t n = length(path);
    uint32_t k;
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < ed_ext_count; i++)
    {
        k = length(ed_exts[i].ext);

        for (j = 0u; j < k && k <= n && lower(path[n - k + j]) == ed_exts[i].ext[j]; j++)
        {
        }

        if (k && j == k)
        {
            return i;
        }
    }

    return ED_NONE;
}

static uint32_t ext_by_tag(const char *tag)
{
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < ed_ext_count && tag[0]; i++)
    {
        for (j = 0u; j < SESS_LANG_BYTES - 1u && tag[j] && tag[j] == ed_exts[i].tag[j]; j++)
        {
        }

        if (!tag[j] && !ed_exts[i].tag[j])
        {
            return i;
        }
    }

    return ED_NONE;
}

static void set_lang(uint32_t tab, uint32_t ext)
{
    struct sess_tab v;
    uint32_t i;

    tabs[tab].lang = ext == ED_NONE ? HL_PLAIN : ed_exts[ext].lang;
    tabs[tab].ext = ext;

    if (sess_tab_get(tab, &v))
    {
        return;
    }

    for (i = 0u; i < SESS_LANG_BYTES; i++)
    {
        v.lang[i] = 0;
    }

    for (i = 0u; ext != ED_NONE && i < SESS_LANG_BYTES - 1u && ed_exts[ext].tag[i]; i++)
    {
        v.lang[i] = ed_exts[ext].tag[i];
    }

    sess_tab_set(tab, &v);
}

static void name_of(uint32_t tab, char *out)
{
    const char *path = sess_tab_path(tab);
    const char *from = path ? leaf(path) : ed_word(ED_W_TAB_NEW);
    struct sess_tab v;
    char digits[10];
    uint32_t n = 0u;
    uint32_t k = 0u;

    while (from[n] && n < NAME_BYTES - 12u)
    {
        out[n] = from[n];
        n++;
    }

    if (!path)
    {
        out[n++] = ' ';
    }

    if (!path && !sess_tab_get(tab, &v))
    {
        do
        {
            digits[k++] = (char)('0' + v.user % 10u);
            v.user /= 10u;
        } while (v.user);

        while (k)
        {
            out[n++] = digits[--k];
        }
    }

    out[n] = 0;
}

static uint32_t free_number(void)
{
    struct sess_tab v;
    uint32_t number = 1u;
    uint32_t pos;

    for (pos = 0u; pos < sess_count(); pos++)
    {
        if (!sess_tab_path(sess_at(pos)) && !sess_tab_get(sess_at(pos), &v) && v.user == number)
        {
            number++;
            pos = ED_NONE;
        }
    }

    return number;
}

static void title(void)
{
    char name[NAME_BYTES];

    told_tab = ed_cur;
    told_unsaved = sess_unsaved(ed_cur);
    name_of(ed_cur, name);
    ed_title(name, told_unsaved);
}

static int slurp(const char *path, uint8_t **bytes, uint32_t *n, uint64_t *mtime)
{
    uint64_t size;
    uint8_t *b;

    if (ed_file_stat(path, &size, mtime) || size > HL_LEN_MAX)
    {
        return -1;
    }

    b = ed_mem_alloc((size_t)size + 1u);

    if (!b)
    {
        return -1;
    }

    if (ed_file_get(path, b, (uint32_t)size))
    {
        ed_mem_free(b);

        return -1;
    }

    *bytes = b;
    *n = (uint32_t)size;

    return 0;
}

static int from_file(const char *path, struct sess_tab *v, uint16_t **text, uint32_t *len)
{
    uint8_t *bytes;
    uint32_t n;
    int bad;

    if (slurp(path, &bytes, &n, &v->disk_mtime))
    {
        return -1;
    }

    v->disk_size = n;
    v->disk_crc = sess_crc(0u, bytes, n);
    bad = ed_decode(bytes, n, 1, text, len, &v->encoding, &v->eol);
    ed_mem_free(bytes);

    return bad;
}

static int from_backup(uint32_t tab, uint16_t **text, uint32_t *len)
{
    uint32_t n = sess_backup_len(tab);
    uint16_t *t = ed_mem_alloc(((size_t)n + 1u) * sizeof *t);

    if (!t)
    {
        return -1;
    }

    if (sess_backup_read(tab, t, n))
    {
        ed_mem_free(t);

        return -1;
    }

    *text = t;
    *len = n;

    return 0;
}

static int changed_on_disk(const char *path, const struct sess_tab *v)
{
    uint64_t size;
    uint64_t mtime;
    uint8_t *bytes;
    uint32_t n;
    int changed;

    if (ed_file_stat(path, &size, &mtime))
    {
        return v->disk_size || v->disk_mtime;
    }

    if (size == v->disk_size && mtime == v->disk_mtime)
    {
        return 0;
    }

    if (slurp(path, &bytes, &n, &mtime))
    {
        return 1;
    }

    changed = n != v->disk_size || sess_crc(0u, bytes, n) != v->disk_crc;
    ed_mem_free(bytes);

    return changed;
}

static void load(uint32_t tab)
{
    struct ed_tab *t = &tabs[tab];
    const char *path = sess_tab_path(tab);
    struct sess_tab v;
    uint16_t *text = 0;
    uint32_t len = 0u;
    int doc;

    sess_tab_get(tab, &v);

    if (sess_unsaved(tab) && !from_backup(tab, &text, &len))
    {
        t->conflict = path && changed_on_disk(path, &v);
    }

    else if (path)
    {
        t->missing = from_file(path, &v, &text, &len) != 0;
        sess_tab_set(tab, &v);
    }

    doc = hl_open(t->lang, text, len);

    if (doc < 0)
    {
        t->lang = HL_PLAIN;
        doc = hl_open(HL_PLAIN, text, len);
    }

    ed_mem_free(text);

    if (doc < 0)
    {
        return;
    }

    t->doc = (uint32_t)doc;
    t->loaded = 1;
    t->mark_off = 0u;
    t->mark_bytes = 0u;
    t->bytes = ed_bytes_before(len);
    t->caret = v.caret > len ? len : v.caret;
    t->anchor = v.anchor > len ? len : v.anchor;
    t->top = v.scroll_row < hl_rows(t->doc) ? v.scroll_row : hl_rows(t->doc) - 1u;
    t->left = (int32_t)(v.scroll_col & 0x7FFFFFFFu);
}

static int save_to(const char *path)
{
    struct sess_tab v;
    uint8_t *bytes;
    uint64_t size;
    uint32_t n;

    ed_now->save_failed = 1;
    sess_tab_get(ed_cur, &v);

    if (ed_encode(ed_now->doc, 0u, hl_len(ed_now->doc), v.encoding, v.eol, &bytes, &n))
    {
        return -1;
    }

    if (ed_file_put(path, bytes, n))
    {
        ed_mem_free(bytes);

        return -1;
    }

    v.disk_size = n;
    v.disk_crc = sess_crc(0u, bytes, n);
    ed_mem_free(bytes);

    if (ed_file_stat(path, &size, &v.disk_mtime))
    {
        v.disk_mtime = 0u;
    }

    sess_tab_set(ed_cur, &v);
    sess_saved(ed_cur);
    ed_now->save_failed = 0;
    ed_now->conflict = 0;
    ed_now->missing = 0;
    title();

    return 0;
}

static void relang(uint32_t ext)
{
    struct ed_tab *t = ed_now;
    const uint16_t *run;
    uint16_t *text;
    uint32_t lang = t->lang;
    uint32_t was = t->ext;
    uint32_t len = hl_len(t->doc);
    uint32_t off;
    uint32_t n;
    uint32_t i;
    int doc;

    set_lang(ed_cur, ext);

    if (t->lang == lang)
    {
        return;
    }

    text = ed_mem_alloc(((size_t)len + 1u) * sizeof *text);
    doc = -1;

    for (off = 0u; text && off < len; off += n)
    {
        run = hl_text(t->doc, off, &n);

        for (i = 0u; i < n; i++)
        {
            text[off + i] = run[i];
        }
    }

    if (text)
    {
        doc = hl_open(t->lang, text, len);
    }

    ed_mem_free(text);

    if (doc < 0)
    {
        set_lang(ed_cur, was);
        return;
    }

    hl_close(t->doc);
    t->doc = (uint32_t)doc;
}

static void drop(void)
{
    if (ed_now->loaded)
    {
        hl_close(ed_now->doc);
    }

    ed_undo_free(&ed_now->undo);
    sess_tab_close(ed_cur);
    ed_now = 0;
    ed_cur = ED_NONE;

    if (!sess_count())
    {
        ed_tab_new(0u, 0);
        return;
    }

    ed_tab_show(sess_active());
}

struct ed_tab *ed_tab_of(uint32_t tab)
{
    return tab < tab_max ? &tabs[tab] : 0;
}

uint32_t ed_tab_name(uint32_t tab, uint16_t *out, uint32_t max)
{
    char name[NAME_BYTES];

    name_of(tab, name);

    return ed_utf8_units(name, length(name), out, max);
}

void ed_tab_title(void)
{
    if (ed_now && (told_tab != ed_cur || told_unsaved != sess_unsaved(ed_cur)))
    {
        title();
    }
}

void ed_tab_sync(void)
{
    struct sess_tab v;

    if (!ed_now || !ed_now->loaded || sess_tab_get(ed_cur, &v))
    {
        return;
    }

    v.caret = ed_now->caret;
    v.anchor = ed_now->anchor;
    v.scroll_row = ed_now->top;
    v.scroll_col = (uint32_t)ed_now->left;
    sess_tab_set(ed_cur, &v);
}

int ed_tabs_adopt(void)
{
    struct sess_tab v;
    uint32_t pos;
    uint32_t tab;

    for (pos = 0u; pos < sess_count(); pos++)
    {
        tab = sess_at(pos);

        if (!room(tab))
        {
            return -1;
        }

        blank(&tabs[tab]);
        sess_tab_get(tab, &v);
        set_lang(tab, ext_by_tag(v.lang));

        if (!sess_tab_path(tab) && !v.user)
        {
            sess_tab_get(tab, &v);
            v.user = free_number();
            sess_tab_set(tab, &v);
        }
    }

    return 0;
}

void ed_tab_show(uint32_t pos)
{
    if (pos >= sess_count())
    {
        return;
    }

    ed_tab_sync();
    sess_set_active(pos);
    ed_cur = sess_at(pos);
    ed_now = &tabs[ed_cur];

    if (!ed_now->loaded)
    {
        load(ed_cur);
    }

    title();
    ed_touch();
}

int ed_tab_new(uint32_t pos, const char *path)
{
    struct sess_tab v;
    int tab = sess_tab_new(pos, path);

    if (tab < 0)
    {
        return -1;
    }

    if (!room((uint32_t)tab))
    {
        sess_tab_close((uint32_t)tab);

        return -1;
    }

    blank(&tabs[tab]);
    sess_tab_get((uint32_t)tab, &v);
    v.encoding = ED_ENC_UTF8;
    v.eol = ed_eol_default;
    v.user = path ? 0u : free_number();
    sess_tab_set((uint32_t)tab, &v);
    set_lang((uint32_t)tab, path ? ext_of(path) : ED_NONE);

    for (pos = 0u; sess_at(pos) != (uint32_t)tab; pos++)
    {
    }

    ed_tab_show(pos);

    return 0;
}

void ed_tab_save(int as)
{
    const char *path = sess_tab_path(ed_cur);

    if (!ed_now->loaded)
    {
        return;
    }

    if (!path || as)
    {
        ed_pick(ED_PICK_SAVE);
        return;
    }

    save_to(path);
    ed_touch();
}

void ed_tab_saved_as(const char *path)
{
    int then_close = ed_close_after_save;

    ed_close_after_save = 0;

    if (sess_tab_set_path(ed_cur, path))
    {
        return;
    }

    relang(ext_of(sess_tab_path(ed_cur)));
    ed_touch();

    if (!save_to(sess_tab_path(ed_cur)) && then_close)
    {
        drop();
    }
}

int ed_tab_close(void)
{
    const char *path = sess_tab_path(ed_cur);
    uint32_t answer = ED_CLOSE_DISCARD;
    char name[NAME_BYTES];

    if (sess_unsaved(ed_cur))
    {
        name_of(ed_cur, name);
        answer = ed_ask_close(name);
    }

    if (answer == ED_CLOSE_CANCEL)
    {
        return 0;
    }

    if (answer == ED_CLOSE_SAVE && !path)
    {
        ed_close_after_save = 1;
        ed_pick(ED_PICK_SAVE);

        return 0;
    }

    if (answer == ED_CLOSE_SAVE && save_to(path))
    {
        ed_touch();

        return 0;
    }

    drop();

    return 1;
}

void ed_tab_save_all(void)
{
    uint32_t back = sess_active();
    uint32_t pos;

    for (pos = 0u; pos < sess_count(); pos++)
    {
        if (!sess_unsaved(sess_at(pos)) || !sess_tab_path(sess_at(pos)))
        {
            continue;
        }

        ed_tab_show(pos);

        if (ed_now->loaded)
        {
            save_to(sess_tab_path(ed_cur));
        }
    }

    ed_tab_show(back);
}

void ed_tab_reload(void)
{
    const char *path = sess_tab_path(ed_cur);
    uint32_t caret = ed_now->caret;
    struct sess_tab v;
    uint16_t *text;
    uint32_t len;

    sess_tab_get(ed_cur, &v);

    if (!path || !ed_now->loaded || from_file(path, &v, &text, &len))
    {
        return;
    }

    ed_replace(0u, hl_len(ed_now->doc), text, len, 0);
    ed_mem_free(text);
    ed_go(caret < len ? caret : len, 0);
    sess_tab_set(ed_cur, &v);
    sess_saved(ed_cur);
    ed_now->conflict = 0;
    ed_now->missing = 0;
    ed_now->save_failed = 0;
}

void ed_tab_lang(uint32_t ext)
{
    if (ed_now->loaded)
    {
        relang(ext);
    }
}
