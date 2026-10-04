#include "sess.h"
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

#define TABS 512u
#define TEXT_MAX 20000u

static const char *dir;
static uint16_t text[TABS][TEXT_MAX];
static uint32_t text_n[TABS];

uint32_t sess_buf_len(uint32_t tab)
{
    return text_n[tab];
}

const uint16_t *sess_buf_run(uint32_t tab, uint32_t off, uint32_t *n)
{
    uint32_t half = text_n[tab] / 2u;

    *n = off < half ? half - off : text_n[tab] - off;

    return text[tab] + off;
}

struct seen_tab {
    uint32_t backup, backup_len, backup_crc, path_len;
    uint32_t caret, anchor, scroll_row, scroll_col, encoding, eol, disk_crc, user;
    uint64_t disk_size, disk_mtime;
    char lang[17];
    char path[300];
};

struct seen {
    uint32_t seq, tabs, active, next_id;
    struct seen_tab tab[TABS];
};

static uint8_t file[4u * 1024u * 1024u];

static uint32_t crc_ref(const uint8_t *p, uint32_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t i;
    uint32_t k;

    for (i = 0u; i < n; i++)
    {
        crc ^= p[i];

        for (k = 0u; k < 8u; k++)
        {
            crc = crc & 1u ? crc >> 1 ^ 0xEDB88320u : crc >> 1;
        }
    }

    return ~crc;
}

static FILE *open_blob(const char *name, const char *mode)
{
    char path[1024];

    snprintf(path, sizeof path, "%s/%s", dir, name);

    return fopen(path, mode);
}

static long slurp(const char *name)
{
    FILE *f = open_blob(name, "rb");
    long n;

    if (!f)
    {
        return -1;
    }

    n = (long)fread(file, 1u, sizeof file, f);
    fclose(f);

    return n;
}

static int exists(const char *name)
{
    return slurp(name) >= 0;
}

static void spill(const char *name, const void *bytes, size_t n)
{
    FILE *f = open_blob(name, "wb");

    fwrite(bytes, 1u, n, f);
    fclose(f);
}

static const char *bname(uint32_t id)
{
    static char name[16];

    snprintf(name, sizeof name, "b%08x", (unsigned)id);

    return name;
}

static int see_slot(int slot, struct seen *m)
{
    struct seen_tab *t;
    const uint8_t *p;
    uint32_t size;
    uint32_t i;
    long n = slurp(slot ? "m1" : "m0");

    if (n < SESS_M_SIZE + 4 || memcmp(file, "SESSMAN1", 8u))
    {
        return 0;
    }

    size = (uint32_t)n;

    if (sess_ld32(file + SESS_M_VERSION) != 1u || sess_ld32(file + SESS_M_BYTES) != size)
    {
        return 0;
    }

    if (sess_ld32(file + size - 4u) != crc_ref(file, size - 4u))
    {
        return 0;
    }

    m->seq = sess_ld32(file + SESS_M_SEQ);
    m->tabs = sess_ld32(file + SESS_M_TABS);
    m->active = sess_ld32(file + SESS_M_ACTIVE);
    m->next_id = sess_ld32(file + SESS_M_NEXT_ID);
    p = file + SESS_M_SIZE;

    for (i = 0u; i < m->tabs && i < TABS; i++)
    {
        t = &m->tab[i];
        t->backup = sess_ld32(p + SESS_T_BACKUP);
        t->backup_len = sess_ld32(p + SESS_T_BACKUP_LEN);
        t->backup_crc = sess_ld32(p + SESS_T_BACKUP_CRC);
        t->path_len = sess_ld32(p + SESS_T_PATH_LEN);
        t->caret = sess_ld32(p + SESS_T_CARET);
        t->anchor = sess_ld32(p + SESS_T_ANCHOR);
        t->scroll_row = sess_ld32(p + SESS_T_SCROLL_ROW);
        t->scroll_col = sess_ld32(p + SESS_T_SCROLL_COL);
        t->encoding = sess_ld32(p + SESS_T_ENCODING);
        t->eol = sess_ld32(p + SESS_T_EOL);
        t->disk_size = sess_ld64(p + SESS_T_DISK_SIZE);
        t->disk_mtime = sess_ld64(p + SESS_T_DISK_MTIME);
        t->disk_crc = sess_ld32(p + SESS_T_DISK_CRC);
        t->user = sess_ld32(p + SESS_T_USER);
        memcpy(t->lang, p + SESS_T_LANG, 16u);
        t->lang[16] = 0;
        memcpy(t->path, p + SESS_T_SIZE, t->path_len < 299u ? t->path_len : 299u);
        t->path[t->path_len < 299u ? t->path_len : 299u] = 0;
        p += SESS_T_SIZE + ((t->path_len + 3u) & ~3u);
    }

    return p == file + size - 4u;
}

static struct seen slot_a;
static struct seen slot_b;

static const struct seen *see(void)
{
    int a = see_slot(0, &slot_a);
    int b = see_slot(1, &slot_b);

    if (a && b)
    {
        return slot_a.seq > slot_b.seq ? &slot_a : &slot_b;
    }

    if (a)
    {
        return &slot_a;
    }

    return b ? &slot_b : 0;
}

static int see_backup(uint32_t id, const uint16_t *want, uint32_t len, uint32_t crc)
{
    uint32_t i;
    long n = slurp(bname(id));

    if (n != (long)(SESS_B_SIZE + len * 2u + 4u) || memcmp(file, "SESSBAK1", 8u))
    {
        return 0;
    }

    if (sess_ld32(file + SESS_B_VERSION) != 1u || sess_ld32(file + SESS_B_ID) != id || sess_ld32(file + SESS_B_LEN) != len)
    {
        return 0;
    }

    for (i = 0u; i < len; i++)
    {
        if (file[SESS_B_SIZE + i * 2u] != (want[i] & 0xFFu) || file[SESS_B_SIZE + i * 2u + 1u] != want[i] >> 8)
        {
            return 0;
        }
    }

    return sess_ld32(file + n - 4) == crc && crc == crc_ref(file, (uint32_t)n - 4u);
}

struct want_tab {
    char path[300];
    struct sess_tab v;
    uint16_t text[64];
    int len;
};

struct want {
    uint32_t n, active;
    struct want_tab tab[32];
};

static struct sess_tab view(uint32_t k)
{
    struct sess_tab v;

    memset(&v, 0, sizeof v);
    snprintf(v.lang, sizeof v.lang, "lang%u", (unsigned)k);
    v.encoding = k + 1u;
    v.eol = k + 2u;
    v.caret = k * 3u;
    v.anchor = k * 5u;
    v.scroll_row = k * 7u;
    v.scroll_col = k * 11u;
    v.disk_size = 0x100000000ull + k;
    v.disk_mtime = 0x0123456789ABCDEFull - k;
    v.disk_crc = 0xDEAD0000u + k;
    v.user = 0xBEEF0000u + k;

    return v;
}

static int view_eq(const struct sess_tab *a, const struct sess_tab *b)
{
    return !memcmp(a->lang, b->lang, sizeof a->lang) && a->encoding == b->encoding && a->eol == b->eol && a->caret == b->caret
        && a->anchor == b->anchor && a->scroll_row == b->scroll_row && a->scroll_col == b->scroll_col
        && a->disk_size == b->disk_size && a->disk_mtime == b->disk_mtime && a->disk_crc == b->disk_crc && a->user == b->user;
}

static void type(uint32_t tab, const char *s)
{
    uint32_t i;

    for (i = 0u; s[i]; i++)
    {
        text[tab][i] = (uint8_t)s[i];
    }

    text_n[tab] = i;
    sess_edited(tab);
}

static void snapshot(struct want *w)
{
    struct want_tab *t;
    uint32_t pos;
    uint32_t id;

    w->n = sess_count();
    w->active = sess_active();

    for (pos = 0u; pos < w->n; pos++)
    {
        t = &w->tab[pos];
        id = sess_at(pos);
        snprintf(t->path, sizeof t->path, "%s", sess_tab_path(id) ? sess_tab_path(id) : "");
        sess_tab_get(id, &t->v);
        t->len = sess_unsaved(id) ? (int)text_n[id] : -1;
        memcpy(t->text, text[id], sizeof t->text);
    }
}

static int holds(const struct want *w)
{
    static uint16_t got[TEXT_MAX];
    const struct want_tab *t;
    struct sess_tab v;
    uint32_t pos;
    uint32_t id;

    if (sess_count() != w->n || sess_active() != w->active)
    {
        return 0;
    }

    for (pos = 0u; pos < w->n; pos++)
    {
        t = &w->tab[pos];
        id = sess_at(pos);

        if (strcmp(t->path, sess_tab_path(id) ? sess_tab_path(id) : "") || sess_tab_get(id, &v) || !view_eq(&v, &t->v))
        {
            return 0;
        }

        if (sess_unsaved(id) != (t->len >= 0))
        {
            return 0;
        }

        if (t->len < 0)
        {
            continue;
        }

        if (sess_backup_len(id) != (uint32_t)t->len || sess_backup_read(id, got, TEXT_MAX) || memcmp(got, t->text, (size_t)t->len * 2u))
        {
            return 0;
        }
    }

    return 1;
}

static int restart(void)
{
    uint32_t pos;
    uint32_t id;
    int n;

    memset(text_n, 0, sizeof text_n);
    store_dir(dir);
    n = sess_load();

    for (pos = 0u; n > 0 && pos < sess_count(); pos++)
    {
        id = sess_at(pos);
        text_n[id] = sess_backup_len(id);

        if (text_n[id] && sess_backup_read(id, text[id], TEXT_MAX))
        {
            text_n[id] = 0u;
        }
    }

    return n;
}

static uint32_t stored(void)
{
    char name[64];
    uint32_t n = 0u;
    int first = 1;

    while (!sess_list(first, name, sizeof name))
    {
        first = 0;
        n++;
    }

    return n;
}

static void t_crc(void)
{
    puts("the crc is the zlib one, in one piece or in several");
    CHECK(sess_crc(0u, "123456789", 9u) == 0xCBF43926u);
    CHECK(sess_crc(sess_crc(0u, "1234", 4u), "56789", 5u) == 0xCBF43926u);
    CHECK(sess_crc(0u, "", 0u) == 0u);
}

static void t_shut(void)
{
    char name[64];

    puts("before sess_load every call is a no-op and nothing is written");
    store_dir(dir);

    while (!sess_list(1, name, sizeof name))
    {
        sess_del(name);
    }

    store_puts = 0u;
    CHECK(sess_tab_new(0u, "x") < 0);
    CHECK(sess_count() == 0u);
    CHECK(sess_at(0u) == SESS_NONE);
    sess_edited(0u);
    CHECK(sess_flush() == 0);
    CHECK(store_puts == 0u && stored() == 0u);
}

static uint32_t tab_a;
static uint32_t tab_b;
static uint32_t tab_c;

static void t_first_run(void)
{
    const struct seen *m;
    struct sess_tab v;

    puts("a fresh directory loads as an empty session, and an idle flush writes nothing");
    CHECK(sess_load() == 0);
    CHECK(sess_count() == 0u && sess_active() == 0u);
    CHECK(sess_flush() == 0 && store_puts == 0u);

    puts("three tabs, one of them typed in: one backup, then one manifest");
    tab_a = (uint32_t)sess_tab_new(0u, "/home/j/a.c");
    tab_b = (uint32_t)sess_tab_new(1u, 0);
    tab_c = (uint32_t)sess_tab_new(99u, "/home/j/caf\xC3\xA9.txt");
    CHECK(sess_count() == 3u && sess_at(0u) == tab_a && sess_at(1u) == tab_b && sess_at(2u) == tab_c);
    v = view(1u);
    CHECK(!sess_tab_set(tab_a, &v));
    v = view(2u);
    CHECK(!sess_tab_set(tab_b, &v));
    type(tab_b, "scratch");
    text[tab_b][3] = 0x1234u;
    sess_set_active(1u);
    CHECK(sess_unsaved(tab_b) && !sess_unsaved(tab_a));
    CHECK(sess_flush() == 0);
    CHECK(store_puts == 2u && store_dels == 0u && stored() == 2u);

    puts("the reader finds seq 1 in slot 1, the records in tab order, the text little-endian");
    m = see();
    CHECK(m && m == &slot_b && !see_slot(0, &slot_a));
    CHECK(m->seq == 1u && m->tabs == 3u && m->active == 1u && m->next_id == 2u);
    CHECK(!strcmp(m->tab[0].path, "/home/j/a.c") && m->tab[0].backup == 0u);
    CHECK(m->tab[0].caret == 3u && m->tab[0].anchor == 5u && m->tab[0].scroll_row == 7u && m->tab[0].scroll_col == 11u);
    CHECK(m->tab[0].encoding == 2u && m->tab[0].eol == 3u && !strcmp(m->tab[0].lang, "lang1"));
    CHECK(m->tab[0].disk_size == 0x100000001ull && m->tab[0].disk_mtime == 0x0123456789ABCDEEull && m->tab[0].disk_crc == 0xDEAD0001u && m->tab[0].user == 0xBEEF0001u);
    CHECK(m->tab[1].path_len == 0u && m->tab[1].backup == 1u && m->tab[1].backup_len == 7u);
    CHECK(!strcmp(m->tab[2].path, "/home/j/caf\xC3\xA9.txt") && m->tab[2].path_len == 17u);
    CHECK(see_backup(1u, text[tab_b], 7u, m->tab[1].backup_crc));
    CHECK(file[SESS_B_SIZE + 6] == 0x34u && file[SESS_B_SIZE + 7] == 0x12u);
}

static void t_cheap(void)
{
    struct sess_tab v;
    const struct seen *m;

    puts("nothing changed: a flush costs nothing");
    CHECK(sess_flush() == 0 && store_puts == 2u);
    sess_set_active(1u);
    sess_tab_get(tab_a, &v);
    CHECK(!sess_tab_set(tab_a, &v));
    CHECK(sess_flush() == 0 && store_puts == 2u);

    puts("the caret moved: one manifest, into the other slot, and no backup is written again");
    v.caret = 77u;
    CHECK(!sess_tab_set(tab_a, &v));
    CHECK(sess_flush() == 0 && store_puts == 3u && stored() == 3u);
    m = see();
    CHECK(m == &slot_a && m->seq == 2u && m->tab[0].caret == 77u && m->tab[1].backup == 1u);
    CHECK(slot_b.seq == 1u);
}

static void t_restart(void)
{
    static uint16_t got[TEXT_MAX];
    struct want w;

    puts("a restart brings every tab back: order, active tab, paths, settings, unsaved text");
    snapshot(&w);
    CHECK(restart() == 3);
    CHECK(holds(&w));
    CHECK(sess_tab_path(sess_at(1u)) == 0 && sess_unsaved(sess_at(1u)));
    CHECK(!sess_backup_read(sess_at(1u), got, 7u) && got[3] == 0x1234u && got[6] == 'h');

    puts("a backup is refused when there is no room for it, or no backup");
    CHECK(sess_backup_read(sess_at(1u), got, 6u) < 0);
    CHECK(sess_backup_read(sess_at(0u), got, TEXT_MAX) < 0);
    CHECK(sess_backup_len(sess_at(0u)) == 0u);

    puts("loading wrote nothing and deleted nothing");
    CHECK(store_puts == 0u || store_puts == 3u);
    CHECK(stored() == 3u);
    tab_a = sess_at(0u);
    tab_b = sess_at(1u);
    tab_c = sess_at(2u);
}

static void t_supersede(void)
{
    const struct seen *m;
    uint32_t dels = store_dels;

    puts("typing again: a backup under a fresh id, the manifest, and only then the old backup goes");
    type(tab_b, "scratch, more");
    CHECK(sess_flush() == 0);
    m = see();
    CHECK(m && m->seq == 3u && m->tab[1].backup == 2u && m->next_id == 3u);
    CHECK(see_backup(2u, text[tab_b], 13u, m->tab[1].backup_crc));
    CHECK(!exists("b00000001") && store_dels == dels + 1u);

    puts("saving a tab drops its backup; closing a tab drops its backup");
    type(tab_a, "edited a");
    CHECK(sess_flush() == 0 && exists("b00000003"));
    sess_saved(tab_a);
    CHECK(!sess_unsaved(tab_a) && exists("b00000003"));
    CHECK(sess_flush() == 0 && !exists("b00000003"));
    sess_tab_close(tab_b);
    CHECK(sess_count() == 2u && sess_active() == 1u && sess_at(1u) == tab_c && exists("b00000002"));
    CHECK(sess_flush() == 0 && !exists("b00000002"));
    CHECK(stored() == 2u);
    m = see();
    CHECK(m && m->tabs == 2u && m->tab[0].backup == 0u && m->tab[1].backup == 0u);
}

static void t_order(void)
{
    struct want w;
    uint32_t d;
    uint32_t e;

    puts("tabs move and the active tab stays the same tab");
    d = (uint32_t)sess_tab_new(0u, "/d");
    e = (uint32_t)sess_tab_new(99u, "/e");
    CHECK(sess_at(0u) == d && sess_at(1u) == tab_a && sess_at(2u) == tab_c && sess_at(3u) == e);
    CHECK(sess_active() == 2u);
    sess_tab_move(tab_c, 0u);
    CHECK(sess_at(0u) == tab_c && sess_at(1u) == d && sess_active() == 0u);
    sess_tab_move(tab_c, 99u);
    CHECK(sess_at(3u) == tab_c && sess_at(2u) == e && sess_active() == 3u);
    sess_tab_move(d, 1u);
    CHECK(sess_at(0u) == tab_a && sess_at(1u) == d && sess_active() == 3u);
    sess_tab_close(tab_a);
    CHECK(sess_active() == 2u && sess_at(2u) == tab_c);
    sess_tab_close(tab_c);
    CHECK(sess_active() == 1u && sess_at(1u) == e);
    CHECK(!sess_tab_set_path(e, "/renamed") && !sess_tab_set_path(d, 0));
    type(d, "d is untitled now");
    CHECK(sess_flush() == 0);
    snapshot(&w);
    CHECK(restart() == 2 && holds(&w));
    CHECK(sess_tab_path(sess_at(0u)) == 0 && !strcmp(sess_tab_path(sess_at(1u)), "/renamed"));
}

static void t_failed_flush(void)
{
    struct want before;
    struct want after;
    uint32_t t0 = sess_at(0u);
    uint32_t t1 = sess_at(1u);
    uint32_t n;

    puts("a flush that fails on a backup leaves storage as it was, and the next one finishes the job");
    snapshot(&before);
    type(t0, "d, second version");
    type(t1, "e has text too");
    snapshot(&after);
    n = stored();
    store_fail(2u);
    CHECK(sess_flush() < 0);
    CHECK(sess_flush() == 0);
    CHECK(stored() == n + 1u);
    CHECK(restart() == 2 && holds(&after));

    puts("a flush that fails on the manifest: the same");
    t0 = sess_at(0u);
    type(t0, "d, third version");
    snapshot(&after);
    store_fail(2u);
    CHECK(sess_flush() < 0);
    CHECK(sess_flush() == 0);
    CHECK(stored() == n + 1u);
    CHECK(restart() == 2 && holds(&after));
    (void)before;
}

static void change_everything(void)
{
    uint32_t fresh;

    type(sess_at(0u), "d, the version after A, a little longer than before");
    sess_saved(sess_at(1u));
    fresh = (uint32_t)sess_tab_new(1u, "/new");
    type(fresh, "brand new");
    sess_tab_move(sess_at(0u), 2u);
    sess_set_active(0u);
}

static void t_die_everywhere(void)
{
    static struct want a;
    static struct want b;
    uint32_t put;
    uint32_t bytes;
    int deaths = 0;
    int as_old = 0;
    int as_new = 0;
    int done;

    puts("dying at every byte of every put of a flush: storage always loads as the old session, whole");
    snapshot(&a);

    for (put = 1u; put <= 3u; put++)
    {
        done = 0;

        for (bytes = 0u; !done && bytes < 4096u; bytes++)
        {
            CHECK(restart() == 2);
            change_everything();
            snapshot(&b);
            store_crash(put, bytes);
            CHECK(sess_flush() < 0);

            done = store_put_bytes < bytes;
            restart();
            deaths++;
            as_old += holds(&a);

            if (holds(&b))
            {
                as_new++;
                done = 1;

                CHECK(put == 3u && see() && bytes == (uint32_t)slurp(see() == &slot_a ? "m0" : "m1"));
            }
        }

        CHECK(done);
    }

    printf("    %d deaths: %d loaded as the old session, %d as the new one\n", deaths, as_old, as_new);
    CHECK(as_old + as_new == deaths && as_new == 1);
    CHECK(deaths > 300);
    CHECK(restart() == 3 && holds(&b));
}

static void t_rot(void)
{
    static struct want older;
    static struct want newer;
    const struct seen *m;
    struct sess_tab v;
    const char *slot;
    uint8_t byte;
    FILE *f;
    long size;
    long i;
    int wrong = 0;

    puts("any one byte of the newest manifest flipped: the older manifest loads, whole");
    snapshot(&older);
    sess_tab_get(sess_at(0u), &v);
    v.caret += 1000u;
    sess_tab_set(sess_at(0u), &v);
    CHECK(sess_flush() == 0);
    snapshot(&newer);
    m = see();
    slot = m == &slot_a ? "m0" : "m1";
    size = slurp(slot);

    for (i = 0; i < size; i++)
    {
        f = open_blob(slot, "r+b");
        fseek(f, i, SEEK_SET);
        byte = (uint8_t)(fgetc(f) ^ 0x40);
        fseek(f, i, SEEK_SET);
        fputc(byte, f);
        fclose(f);
        restart();
        wrong += !holds(&older);
        f = open_blob(slot, "r+b");
        fseek(f, i, SEEK_SET);
        fputc(byte ^ 0x40, f);
        fclose(f);
    }

    CHECK(wrong == 0);
    CHECK(restart() == 3 && holds(&newer));

    puts("a manifest cut short, or with bytes added, is no manifest");
    slurp(slot);
    spill("keep", file, (size_t)size);
    spill(slot, file, (size_t)size - 1u);
    restart();
    CHECK(holds(&older));
    slurp("keep");
    file[size] = 0u;
    spill(slot, file, (size_t)size + 1u);
    restart();
    CHECK(holds(&older));
    slurp("keep");
    spill(slot, file, (size_t)size);
    sess_del("keep");
    CHECK(restart() == 3 && holds(&newer));
}

static void t_bad_backup(void)
{
    static uint16_t got[TEXT_MAX];
    uint32_t tab = sess_at(0u);
    const struct seen *m = see();
    uint32_t id = m->tab[0].backup;
    long size = slurp(bname(id));

    puts("a backup with one byte wrong is refused, not handed out");
    CHECK(sess_unsaved(tab) && id && !sess_backup_read(tab, got, TEXT_MAX));
    spill("keep", file, (size_t)size);
    file[SESS_B_SIZE + 3] ^= 1u;
    spill(bname(id), file, (size_t)size);
    CHECK(sess_backup_read(tab, got, TEXT_MAX) < 0);
    slurp("keep");
    spill(bname(id), file, (size_t)size - 2u);
    CHECK(sess_backup_read(tab, got, TEXT_MAX) < 0);
    slurp("keep");
    spill(bname(id), file, (size_t)size);
    sess_del("keep");
    CHECK(!sess_backup_read(tab, got, TEXT_MAX));
}

static void t_strays(void)
{
    static struct want w;

    puts("a backup nothing points at is deleted at load; a file that is not the session's is left alone");
    snapshot(&w);
    spill("b000000ff", "junk", 4u);
    spill("b0000zzzz", "not a backup name", 17u);
    spill("notes.txt", "mine", 4u);
    CHECK(restart() == 3 && holds(&w));
    CHECK(!exists("b000000ff") && exists("b0000zzzz") && exists("notes.txt"));
    sess_del("b0000zzzz");
    sess_del("notes.txt");
}

static void t_no_index(void)
{
    static uint16_t got[TEXT_MAX];
    static struct want w;
    uint32_t n = 0u;
    uint32_t pos;
    uint32_t files;

    puts("both manifests gone: no backup is deleted, and every unsaved text comes back as an untitled tab");
    snapshot(&w);

    for (pos = 0u; pos < w.n; pos++)
    {
        n += w.tab[pos].len >= 0;
    }

    CHECK(n == 2u);
    spill("m0", "x", 1u);
    sess_del("m1");
    spill("b00000050", "SESSBAK1 but torn", 17u);
    files = stored();
    CHECK(restart() == 2);
    CHECK(stored() == files);

    for (pos = 0u; pos < sess_count(); pos++)
    {
        CHECK(sess_unsaved(sess_at(pos)) && sess_tab_path(sess_at(pos)) == 0);
        CHECK(!sess_backup_read(sess_at(pos), got, TEXT_MAX));
        CHECK(got[0] == 'd' || got[0] == 'b');
    }

    puts("the next flush gives them an index again, under ids no old backup has");
    type(sess_at(0u), "typed after the recovery");
    CHECK(sess_flush() == 0);
    CHECK(see() && see()->tabs == 2u && see()->next_id > 0x51u);
    snapshot(&w);
    CHECK(restart() == 2 && holds(&w));
    CHECK(!exists("b00000050"));
}

static void t_no_memory(void)
{
    static struct want w;
    uint32_t files;
    uint32_t n;

    puts("no memory for a tab: nothing is added");
    snapshot(&w);
    hosted_port_fail_alloc(1u);
    CHECK(sess_tab_new(0u, "/nope") < 0);
    hosted_port_fail_alloc(1u);
    CHECK(sess_tab_set_path(sess_at(0u), "/nope") < 0);
    CHECK(holds(&w));

    puts("no memory at load: the session stays shut and storage is untouched");
    files = stored();
    memset(text_n, 0, sizeof text_n);
    store_dir(dir);

    for (n = 1u; n <= 2u; n++)
    {
        hosted_port_fail_alloc(n);
        CHECK(sess_load() < 0);
        CHECK(sess_count() == 0u && sess_tab_new(0u, 0) < 0 && sess_flush() == 0);
    }

    hosted_port_fail_alloc(0u);
    CHECK(stored() == files);
    CHECK(restart() == 2 && holds(&w));
}

static void t_many(void)
{
    static uint16_t got[TEXT_MAX];
    char path[300];
    uint32_t i;
    uint32_t id;
    int wrong = 0;

    puts("three hundred tabs with paths of every length, and a text many sectors long");

    while (sess_count())
    {
        sess_tab_close(sess_at(0u));
    }

    for (i = 0u; i < 300u; i++)
    {
        memset(path, 'p', i);
        path[i] = 0;
        id = (uint32_t)sess_tab_new(i, i ? path : 0);
        wrong += id >= TABS;
    }

    id = sess_at(150u);

    for (i = 0u; i < TEXT_MAX; i++)
    {
        text[id][i] = (uint16_t)(i * 2654435761u >> 7);
    }

    text_n[id] = TEXT_MAX;
    sess_edited(id);
    sess_set_active(299u);
    CHECK(sess_flush() == 0);
    CHECK(restart() == 300 && sess_active() == 299u);

    for (i = 0u; i < 300u; i++)
    {
        memset(path, 'p', i);
        path[i] = 0;
        id = sess_at(i);
        wrong += i ? strcmp(sess_tab_path(id), path) != 0 : sess_tab_path(id) != 0;
        wrong += sess_unsaved(id) != (i == 150u);
    }

    CHECK(wrong == 0);
    id = sess_at(150u);
    CHECK(sess_backup_len(id) == TEXT_MAX && !sess_backup_read(id, got, TEXT_MAX));

    for (i = 0u; i < TEXT_MAX; i++)
    {
        wrong += got[i] != (uint16_t)(i * 2654435761u >> 7);
    }

    CHECK(wrong == 0);
}

static void t_empty_again(void)
{
    puts("every tab closed: storage is one small manifest per slot, and the session holds no memory");

    while (sess_count())
    {
        sess_tab_close(sess_at(0u));
    }

    CHECK(sess_flush() == 0);
    CHECK(stored() == 2u && see() && see()->tabs == 0u);
    CHECK(restart() == 0);
    CHECK(hosted_port_live == 0u);
    sess_del("m0");
    sess_del("m1");
}

int main(int argc, char **argv)
{
    dir = argc > 1 ? argv[1] : "sess_test.tmp";
    t_crc();
    t_shut();
    t_first_run();
    t_cheap();
    t_restart();
    t_supersede();
    t_order();
    t_failed_flush();
    t_die_everywhere();
    t_rot();
    t_bad_backup();
    t_strays();
    t_no_index();
    t_no_memory();
    t_many();
    t_empty_again();
    printf("%d checks, %d failed\n", checks, fails);

    return fails ? 1 : 0;
}
