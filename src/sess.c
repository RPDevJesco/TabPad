#include "sess.h"

#define SEC 4096u
#define NAME 10u
#define TABS_FIRST 16u
#define DEAD_FIRST 16u

struct tab {
    struct sess_tab t;
    char *path;
    uint32_t path_len;
    uint32_t backup, backup_len, backup_crc;
    int used, unsaved, stale;
};

static struct tab *tabs;
static uint32_t tab_max;
static uint32_t *order;
static uint32_t count;
static uint32_t active;
static uint32_t *dead;
static uint32_t dead_n;
static uint32_t dead_max;
static uint32_t seq;
static uint32_t next_id;
static int ready;
static int changed;
static int starved;
static uint8_t sec[SEC];
static uint32_t w_crc;
static uint32_t w_fill;
static int w_opened;
static int w_bad;
static uint32_t r_crc;

static void backup_name(uint32_t id, char *name)
{
    uint32_t i;

    name[0] = 'b';

    for (i = 0u; i < 8u; i++)
    {
        name[1u + i] = "0123456789abcdef"[id >> (28u - 4u * i) & 15u];
    }

    name[9] = 0;
}

static int backup_id(const char *name, uint32_t *id)
{
    uint32_t v = 0u;
    uint32_t i;
    char c;

    if (name[0] != 'b')
    {
        return 0;
    }

    for (i = 1u; i < 9u; i++)
    {
        c = name[i];

        if (c >= '0' && c <= '9')
        {
            v = v << 4 | (uint32_t)(c - '0');
        }

        else if (c >= 'a' && c <= 'f')
        {
            v = v << 4 | (uint32_t)(c - 'a' + 10);
        }

        else
        {
            return 0;
        }
    }

    *id = v;

    return !name[9];
}

static void slot_name(uint32_t slot, char *name)
{
    name[0] = 'm';
    name[1] = (char)('0' + (slot & 1u));
    name[2] = 0;
}

static uint32_t pad4(uint32_t n)
{
    return (n + 3u) & ~3u;
}

static void w_flush(void)
{
    w_crc = sess_crc(w_crc, sec, w_fill);

    if (!w_bad && w_fill && sess_put_write(sec, w_fill))
    {
        w_bad = 1;
    }

    w_fill = 0u;
}

static void w_open(const char *name)
{
    w_crc = 0u;
    w_fill = 0u;
    w_opened = !sess_put_open(name);
    w_bad = !w_opened;
}

static void w_bytes(const uint8_t *p, uint32_t n)
{
    uint32_t i;

    for (i = 0u; i < n; i++)
    {
        if (w_fill == SEC)
        {
            w_flush();
        }

        sec[w_fill++] = p[i];
    }
}

static void w_zeros(uint32_t n)
{
    static const uint8_t zero[4];

    w_bytes(zero, n);
}

static int w_close(const char *name)
{
    uint8_t tail[4];

    w_flush();
    sess_st32(tail, w_crc);

    if (!w_bad && sess_put_write(tail, sizeof tail))
    {
        w_bad = 1;
    }

    if (w_opened && sess_put_close())
    {
        w_bad = 1;
    }

    if (w_bad)
    {
        sess_del(name);
        return -1;
    }

    return 0;
}

static int r_bytes(uint8_t *p, uint32_t n)
{
    if (sess_get_read(p, n))
    {
        return -1;
    }

    r_crc = sess_crc(r_crc, p, n);

    return 0;
}

static int r_crc_holds(void)
{
    uint8_t tail[4];

    return !sess_get_read(tail, sizeof tail) && sess_ld32(tail) == r_crc;
}

static struct tab *get(uint32_t tab)
{
    return ready && tab < tab_max && tabs[tab].used ? &tabs[tab] : 0;
}

static void bury(uint32_t id)
{
    uint32_t *grown;
    uint32_t max;
    uint32_t i;

    if (!id)
    {
        return;
    }

    if (dead_n == dead_max)
    {
        max = dead_max ? dead_max * 2u : DEAD_FIRST;
        grown = sess_mem_alloc((size_t)max * sizeof *grown);

        if (!grown)
        {
            return;
        }

        for (i = 0u; i < dead_n; i++)
        {
            grown[i] = dead[i];
        }

        sess_mem_free(dead);
        dead = grown;
        dead_max = max;
    }

    dead[dead_n++] = id;
}

static void burn(void)
{
    char name[NAME];
    uint32_t i;

    for (i = 0u; i < dead_n; i++)
    {
        backup_name(dead[i], name);
        sess_del(name);
    }

    dead_n = 0u;
}

static int tab_room(void)
{
    struct tab *grown;
    uint32_t *grown_order;
    uint32_t max;
    uint32_t i;

    if (count < tab_max)
    {
        return 1;
    }

    max = tab_max ? tab_max * 2u : TABS_FIRST;
    grown = sess_mem_alloc((size_t)max * sizeof *grown);
    grown_order = sess_mem_alloc((size_t)max * sizeof *grown_order);

    if (!grown || !grown_order)
    {
        sess_mem_free(grown);
        sess_mem_free(grown_order);

        return 0;
    }

    for (i = 0u; i < tab_max; i++)
    {
        grown[i] = tabs[i];
    }

    for (i = tab_max; i < max; i++)
    {
        grown[i].used = 0;
    }

    for (i = 0u; i < count; i++)
    {
        grown_order[i] = order[i];
    }

    sess_mem_free(tabs);
    sess_mem_free(order);
    tabs = grown;
    order = grown_order;
    tab_max = max;

    return 1;
}

static char *path_copy(const char *path, uint32_t len)
{
    char *p = sess_mem_alloc((size_t)len + 1u);
    uint32_t i;

    if (!p)
    {
        return 0;
    }

    for (i = 0u; i < len; i++)
    {
        p[i] = path[i];
    }

    p[len] = 0;

    return p;
}

static uint32_t text_len(const char *s)
{
    uint32_t n = 0u;

    while (s && s[n])
    {
        n++;
    }

    return n;
}

static uint32_t pos_of(uint32_t tab)
{
    uint32_t pos;

    for (pos = 0u; pos < count; pos++)
    {
        if (order[pos] == tab)
        {
            return pos;
        }
    }

    return SESS_NONE;
}

static struct tab *tab_add(uint32_t pos, char *path, uint32_t path_len)
{
    static const struct sess_tab blank;
    struct tab *t;
    uint32_t id = 0u;
    uint32_t i;

    if (!tab_room())
    {
        return 0;
    }

    while (tabs[id].used)
    {
        id++;
    }

    pos = pos > count ? count : pos;

    for (i = count; i > pos; i--)
    {
        order[i] = order[i - 1u];
    }

    order[pos] = id;

    if (count && pos <= active)
    {
        active++;
    }

    count++;
    t = &tabs[id];
    t->t = blank;
    t->path = path;
    t->path_len = path_len;
    t->backup = 0u;
    t->backup_len = 0u;
    t->backup_crc = 0u;
    t->used = 1;
    t->unsaved = 0;
    t->stale = 0;

    return t;
}

static void reset(void)
{
    uint32_t i;

    for (i = 0u; i < tab_max; i++)
    {
        if (tabs[i].used)
        {
            sess_mem_free(tabs[i].path);
        }
    }

    sess_mem_free(tabs);
    sess_mem_free(order);
    sess_mem_free(dead);
    tabs = 0;
    order = 0;
    dead = 0;
    tab_max = 0u;
    count = 0u;
    active = 0u;
    dead_n = 0u;
    dead_max = 0u;
    seq = 0u;
    next_id = 1u;
    ready = 0;
    changed = 0;
}

static int ship(uint32_t tab)
{
    struct tab *t = &tabs[tab];
    const uint16_t *run;
    uint8_t h[SESS_B_SIZE];
    char name[NAME];
    uint32_t len = sess_buf_len(tab);
    uint32_t id = next_id++;
    uint32_t off;
    uint32_t n;
    uint32_t i;

    backup_name(id, name);
    w_open(name);
    w_bytes((const uint8_t *)"SESSBAK1", 8u);
    sess_st32(h + SESS_B_VERSION, SESS_VERSION);
    sess_st32(h + SESS_B_ID, id);
    sess_st32(h + SESS_B_LEN, len);
    sess_st32(h + SESS_B_RESERVED, 0u);
    w_bytes(h + SESS_B_VERSION, SESS_B_SIZE - SESS_B_VERSION);

    for (off = 0u; off < len && !w_bad; off += n)
    {
        run = sess_buf_run(tab, off, &n);

        if (!run || !n)
        {
            w_bad = 1;
            break;
        }

        n = n > len - off ? len - off : n;

        for (i = 0u; i < n; i++)
        {
            sess_st16(h, run[i]);
            w_bytes(h, 2u);
        }
    }

    if (w_close(name))
    {
        return -1;
    }

    bury(t->backup);
    t->backup = id;
    t->backup_len = len;
    t->backup_crc = w_crc;
    t->stale = 0;

    return 0;
}

static void ship_record(const struct tab *t)
{
    uint8_t r[SESS_T_SIZE];
    uint32_t i;

    sess_st32(r + SESS_T_BACKUP, t->backup);
    sess_st32(r + SESS_T_BACKUP_LEN, t->backup_len);
    sess_st32(r + SESS_T_BACKUP_CRC, t->backup_crc);
    sess_st32(r + SESS_T_PATH_LEN, t->path_len);
    sess_st32(r + SESS_T_CARET, t->t.caret);
    sess_st32(r + SESS_T_ANCHOR, t->t.anchor);
    sess_st32(r + SESS_T_SCROLL_ROW, t->t.scroll_row);
    sess_st32(r + SESS_T_SCROLL_COL, t->t.scroll_col);
    sess_st32(r + SESS_T_ENCODING, t->t.encoding);
    sess_st32(r + SESS_T_EOL, t->t.eol);
    sess_st64(r + SESS_T_DISK_SIZE, t->t.disk_size);
    sess_st64(r + SESS_T_DISK_MTIME, t->t.disk_mtime);
    sess_st32(r + SESS_T_DISK_CRC, t->t.disk_crc);
    sess_st32(r + SESS_T_USER, t->t.user);

    for (i = 0u; i < SESS_LANG_BYTES; i++)
    {
        r[SESS_T_LANG + i] = (uint8_t)t->t.lang[i];
    }

    w_bytes(r, sizeof r);
    w_bytes((const uint8_t *)t->path, t->path_len);
    w_zeros(pad4(t->path_len) - t->path_len);
}

static int ship_manifest(void)
{
    uint8_t h[SESS_M_SIZE];
    char name[NAME];
    uint32_t bytes = SESS_M_SIZE + 4u;
    uint32_t pos;

    for (pos = 0u; pos < count; pos++)
    {
        bytes += SESS_T_SIZE + pad4(tabs[order[pos]].path_len);
    }

    slot_name(seq + 1u, name);
    w_open(name);
    w_bytes((const uint8_t *)"SESSMAN1", 8u);
    sess_st32(h + SESS_M_VERSION, SESS_VERSION);
    sess_st32(h + SESS_M_SEQ, seq + 1u);
    sess_st32(h + SESS_M_TABS, count);
    sess_st32(h + SESS_M_ACTIVE, active);
    sess_st32(h + SESS_M_NEXT_ID, next_id);
    sess_st32(h + SESS_M_BYTES, bytes);
    w_bytes(h + SESS_M_VERSION, SESS_M_SIZE - SESS_M_VERSION);

    for (pos = 0u; pos < count; pos++)
    {
        ship_record(&tabs[order[pos]]);
    }

    return w_close(name);
}

static int do_flush(void)
{
    uint32_t pos;

    for (pos = 0u; pos < count; pos++)
    {
        if (tabs[order[pos]].stale && ship(order[pos]))
        {
            return -1;
        }
    }

    if (ship_manifest())
    {
        return -1;
    }

    seq++;
    changed = 0;
    burn();

    return 0;
}

static int r_skip(uint32_t n)
{
    uint32_t k;

    while (n)
    {
        k = n > SEC ? SEC : n;

        if (r_bytes(sec, k))
        {
            return -1;
        }

        n -= k;
    }

    return 0;
}

static int load_record(int keep, uint32_t *left)
{
    uint8_t r[SESS_T_SIZE];
    struct tab *t;
    char *path = 0;
    uint32_t len;
    uint32_t i;

    if (*left < SESS_T_SIZE || r_bytes(r, sizeof r))
    {
        return -1;
    }

    len = sess_ld32(r + SESS_T_PATH_LEN);

    if (len > *left - SESS_T_SIZE || pad4(len) > *left - SESS_T_SIZE)
    {
        return -1;
    }

    *left -= SESS_T_SIZE + pad4(len);

    if (!keep)
    {
        return r_skip(pad4(len));
    }

    if (len)
    {
        path = sess_mem_alloc((size_t)len + 1u);

        if (!path)
        {
            starved = 1;

            return -1;
        }

        path[len] = 0;
    }

    if ((len && r_bytes((uint8_t *)path, len)) || r_skip(pad4(len) - len))
    {
        sess_mem_free(path);

        return -1;
    }

    t = tab_add(count, path, len);

    if (!t)
    {
        sess_mem_free(path);
        starved = 1;

        return -1;
    }

    t->backup = sess_ld32(r + SESS_T_BACKUP);
    t->backup_len = sess_ld32(r + SESS_T_BACKUP_LEN);
    t->backup_crc = sess_ld32(r + SESS_T_BACKUP_CRC);
    t->unsaved = t->backup != 0u;
    t->t.caret = sess_ld32(r + SESS_T_CARET);
    t->t.anchor = sess_ld32(r + SESS_T_ANCHOR);
    t->t.scroll_row = sess_ld32(r + SESS_T_SCROLL_ROW);
    t->t.scroll_col = sess_ld32(r + SESS_T_SCROLL_COL);
    t->t.encoding = sess_ld32(r + SESS_T_ENCODING);
    t->t.eol = sess_ld32(r + SESS_T_EOL);
    t->t.disk_size = sess_ld64(r + SESS_T_DISK_SIZE);
    t->t.disk_mtime = sess_ld64(r + SESS_T_DISK_MTIME);
    t->t.disk_crc = sess_ld32(r + SESS_T_DISK_CRC);
    t->t.user = sess_ld32(r + SESS_T_USER);

    for (i = 0u; i < SESS_LANG_BYTES; i++)
    {
        t->t.lang[i] = (char)r[SESS_T_LANG + i];
    }

    return 0;
}

static uint32_t slot_body(uint32_t size, int keep)
{
    uint8_t h[SESS_M_SIZE];
    uint32_t left = size - SESS_M_SIZE - 4u;
    uint32_t n;
    uint32_t i;

    r_crc = 0u;

    if (r_bytes(h, sizeof h) || !sess_magic_eq(h, "SESSMAN1"))
    {
        return 0u;
    }

    if (sess_ld32(h + SESS_M_VERSION) != SESS_VERSION || sess_ld32(h + SESS_M_BYTES) != size)
    {
        return 0u;
    }

    n = sess_ld32(h + SESS_M_TABS);

    for (i = 0u; i < n; i++)
    {
        if (load_record(keep, &left))
        {
            return 0u;
        }
    }

    if (left || !r_crc_holds())
    {
        return 0u;
    }

    if (keep)
    {
        active = sess_ld32(h + SESS_M_ACTIVE);
        next_id = sess_ld32(h + SESS_M_NEXT_ID);
    }

    return sess_ld32(h + SESS_M_SEQ);
}

static uint32_t slot_read(uint32_t slot, int keep)
{
    char name[NAME];
    uint32_t size;
    uint32_t got = 0u;

    slot_name(slot, name);

    if (sess_get_open(name, &size))
    {
        return 0u;
    }

    if (size >= SESS_M_SIZE + 4u)
    {
        got = slot_body(size, keep);
    }

    sess_get_close();

    return got;
}

static int referenced(uint32_t id)
{
    uint32_t i;

    for (i = 0u; i < tab_max; i++)
    {
        if (tabs[i].used && tabs[i].backup == id)
        {
            return 1;
        }
    }

    return 0;
}

static int backup_whole(uint32_t id, uint32_t *len, uint32_t *crc)
{
    uint8_t h[SESS_B_SIZE];
    char name[NAME];
    uint32_t size;
    int whole;

    backup_name(id, name);

    if (sess_get_open(name, &size))
    {
        return 0;
    }

    r_crc = 0u;
    whole = size >= SESS_B_SIZE + 4u && !r_bytes(h, sizeof h) && sess_magic_eq(h, "SESSBAK1");
    whole = whole && sess_ld32(h + SESS_B_VERSION) == SESS_VERSION && sess_ld32(h + SESS_B_ID) == id;
    *len = whole ? sess_ld32(h + SESS_B_LEN) : 0u;
    whole = whole && *len == (size - SESS_B_SIZE - 4u) / 2u && !(size & 1u) && !r_skip(*len * 2u);
    *crc = r_crc;
    whole = whole && r_crc_holds();
    sess_get_close();

    return whole;
}

static void recover(void)
{
    char name[64];
    struct tab *t;
    uint32_t id;
    uint32_t len;
    uint32_t crc;
    uint32_t i;
    int first = 1;

    while (!sess_list(first, name, sizeof name))
    {
        first = 0;

        if (backup_id(name, &id) && id)
        {
            bury(id);
            next_id = id >= next_id ? id + 1u : next_id;
        }
    }

    for (i = 0u; i < dead_n; i++)
    {
        if (!backup_whole(dead[i], &len, &crc))
        {
            continue;
        }

        t = tab_add(count, 0, 0u);

        if (!t)
        {
            break;
        }

        t->backup = dead[i];
        t->backup_len = len;
        t->backup_crc = crc;
        t->unsaved = 1;
        changed = 1;
    }

    dead_n = 0u;
}

static void sweep(void)
{
    char name[64];
    uint32_t id;
    int first = 1;

    while (!sess_list(first, name, sizeof name))
    {
        first = 0;

        if (backup_id(name, &id) && !referenced(id))
        {
            bury(id);
        }
    }

    burn();
}

static int do_load(void)
{
    uint32_t seqs[2];
    uint32_t slot;
    uint32_t tries;
    uint32_t i;

    seqs[0] = slot_read(0u, 0);
    seqs[1] = slot_read(1u, 0);
    slot = seqs[1] > seqs[0] ? 1u : 0u;

    for (tries = 0u; tries < 2u && seqs[slot]; tries++)
    {
        starved = 0;
        seq = slot_read(slot, 1);

        if (seq)
        {
            break;
        }

        reset();

        if (starved)
        {
            return -1;
        }

        slot ^= 1u;
    }

    active = active < count ? active : 0u;

    for (i = 0u; i < tab_max; i++)
    {
        if (tabs[i].used && tabs[i].backup >= next_id)
        {
            next_id = tabs[i].backup + 1u;
        }
    }

    changed = 0;

    if (seq)
    {
        sweep();
    }

    else
    {
        recover();
    }

    ready = 1;

    return (int)count;
}

int sess_load(void)
{
    reset();

    return do_load();
}

int sess_flush(void)
{
    if (!ready || !changed)
    {
        return 0;
    }

    return do_flush();
}

uint32_t sess_count(void)
{
    return ready ? count : 0u;
}

uint32_t sess_at(uint32_t pos)
{
    return ready && pos < count ? order[pos] : SESS_NONE;
}

uint32_t sess_active(void)
{
    return ready ? active : 0u;
}

void sess_set_active(uint32_t pos)
{
    if (!ready || pos >= count || pos == active)
    {
        return;
    }

    active = pos;
    changed = 1;
}

int sess_tab_new(uint32_t pos, const char *path)
{
    struct tab *t;
    char *copy = 0;
    uint32_t len = text_len(path);

    if (!ready)
    {
        return -1;
    }

    if (len)
    {
        copy = path_copy(path, len);

        if (!copy)
        {
            return -1;
        }
    }

    t = tab_add(pos, copy, len);

    if (!t)
    {
        sess_mem_free(copy);

        return -1;
    }

    changed = 1;

    return (int)(t - tabs);
}

void sess_tab_close(uint32_t tab)
{
    struct tab *t = get(tab);
    uint32_t pos;
    uint32_t i;

    if (!t)
    {
        return;
    }

    pos = pos_of(tab);
    count--;

    for (i = pos; i < count; i++)
    {
        order[i] = order[i + 1u];
    }

    if (pos < active || (active && active == count))
    {
        active--;
    }

    bury(t->backup);
    sess_mem_free(t->path);
    t->used = 0;
    changed = 1;
}

void sess_tab_move(uint32_t tab, uint32_t pos)
{
    uint32_t was_active;
    uint32_t from;
    uint32_t i;

    if (!get(tab))
    {
        return;
    }

    was_active = order[active];
    from = pos_of(tab);
    pos = pos >= count ? count - 1u : pos;

    for (i = from; i < pos; i++)
    {
        order[i] = order[i + 1u];
    }

    for (i = from; i > pos; i--)
    {
        order[i] = order[i - 1u];
    }

    order[pos] = tab;
    active = pos_of(was_active);
    changed = from != pos ? 1 : changed;
}

int sess_tab_get(uint32_t tab, struct sess_tab *out)
{
    const struct tab *t = get(tab);

    if (!t)
    {
        return -1;
    }

    *out = t->t;

    return 0;
}

static int same(const struct sess_tab *a, const struct sess_tab *b)
{
    uint32_t i;

    for (i = 0u; i < SESS_LANG_BYTES; i++)
    {
        if (a->lang[i] != b->lang[i])
        {
            return 0;
        }
    }

    return a->encoding == b->encoding && a->eol == b->eol && a->caret == b->caret && a->anchor == b->anchor
        && a->scroll_row == b->scroll_row && a->scroll_col == b->scroll_col && a->disk_size == b->disk_size
        && a->disk_mtime == b->disk_mtime && a->disk_crc == b->disk_crc && a->user == b->user;
}

int sess_tab_set(uint32_t tab, const struct sess_tab *in)
{
    struct tab *t = get(tab);

    if (!t)
    {
        return -1;
    }

    if (same(&t->t, in))
    {
        return 0;
    }

    t->t = *in;
    changed = 1;

    return 0;
}

const char *sess_tab_path(uint32_t tab)
{
    const struct tab *t = get(tab);

    return t ? t->path : 0;
}

int sess_tab_set_path(uint32_t tab, const char *path)
{
    struct tab *t = get(tab);
    char *copy = 0;
    uint32_t len = text_len(path);

    if (!t)
    {
        return -1;
    }

    if (len)
    {
        copy = path_copy(path, len);

        if (!copy)
        {
            return -1;
        }
    }

    sess_mem_free(t->path);
    t->path = copy;
    t->path_len = len;
    changed = 1;

    return 0;
}

void sess_edited(uint32_t tab)
{
    struct tab *t = get(tab);

    if (!t)
    {
        return;
    }

    t->unsaved = 1;
    t->stale = 1;
    changed = 1;
}

void sess_saved(uint32_t tab)
{
    struct tab *t = get(tab);

    if (!t)
    {
        return;
    }

    bury(t->backup);
    t->backup = 0u;
    t->backup_len = 0u;
    t->backup_crc = 0u;
    t->unsaved = 0;
    t->stale = 0;
    changed = 1;
}

int sess_unsaved(uint32_t tab)
{
    const struct tab *t = get(tab);

    return t ? t->unsaved : 0;
}

uint32_t sess_backup_len(uint32_t tab)
{
    const struct tab *t = get(tab);

    return t && t->backup ? t->backup_len : 0u;
}

static int backup_body(const struct tab *t, uint32_t size, uint16_t *out)
{
    uint8_t h[SESS_B_SIZE];
    uint32_t left = t->backup_len;
    uint32_t n;
    uint32_t i;

    r_crc = 0u;

    if (size != SESS_B_SIZE + 4u + t->backup_len * 2u || r_bytes(h, sizeof h) || !sess_magic_eq(h, "SESSBAK1"))
    {
        return -1;
    }

    if (sess_ld32(h + SESS_B_VERSION) != SESS_VERSION || sess_ld32(h + SESS_B_ID) != t->backup || sess_ld32(h + SESS_B_LEN) != t->backup_len)
    {
        return -1;
    }

    while (left)
    {
        n = left > SEC / 2u ? SEC / 2u : left;

        if (r_bytes(sec, n * 2u))
        {
            return -1;
        }

        for (i = 0u; i < n; i++)
        {
            out[i] = (uint16_t)sess_ld16(sec + i * 2u);
        }

        out += n;
        left -= n;
    }

    return r_crc_holds() && r_crc == t->backup_crc ? 0 : -1;
}

int sess_backup_read(uint32_t tab, uint16_t *out, uint32_t max)
{
    const struct tab *t = get(tab);
    char name[NAME];
    uint32_t size;
    int bad;

    if (!t || !t->backup || !out || max < t->backup_len)
    {
        return -1;
    }

    backup_name(t->backup, name);

    if (sess_get_open(name, &size))
    {
        return -1;
    }

    bad = backup_body(t, size, out);
    sess_get_close();

    return bad;
}
