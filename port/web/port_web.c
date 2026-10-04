#include "ed.h"
#include "hl.h"
#include "sess.h"

#define PAGE 65536u
#define CLASSES 28u
#define SMALLEST 16u
#define HEAD 8u
#define GROW_PAGES 64u

struct head {
    uint32_t class;
    uint32_t unused;
};

struct spare {
    struct spare *next;
};

extern unsigned char __heap_base;
extern const unsigned char web_tongue_zh_cn[];
extern const unsigned web_tongue_zh_cn_size;

int web_confirm(const char *text);
void web_erase(void);
void web_stop(void);

const uint32_t ed_eol_default = ED_EOL_LF;
const int ed_erase_offered = 1;

static struct spare *spares[CLASSES];
static uintptr_t heap_at;
static uintptr_t heap_end;

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *web_alloc(uint32_t n);
void web_free(void *p);

void *memcpy(void *dst, const void *src, size_t n)
{
    return __builtin_memcpy(dst, src, n);
}

void *memmove(void *dst, const void *src, size_t n)
{
    return __builtin_memmove(dst, src, n);
}

void *memset(void *dst, int c, size_t n)
{
    return __builtin_memset(dst, c, n);
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a;
    const unsigned char *y = b;
    size_t i;

    for (i = 0u; i < n; i++)
    {
        if (x[i] != y[i])
        {
            return x[i] < y[i] ? -1 : 1;
        }
    }

    return 0;
}

static void *carve(uint32_t size)
{
    uintptr_t at;
    uint32_t pages;

    if (!heap_at)
    {
        heap_at = ((uintptr_t)&__heap_base + 15u) & ~(uintptr_t)15u;
        heap_end = (uintptr_t)__builtin_wasm_memory_size(0) * PAGE;
    }

    if (size > heap_end - heap_at)
    {
        pages = (size - (uint32_t)(heap_end - heap_at) + PAGE - 1u) / PAGE;
        pages = pages < GROW_PAGES ? GROW_PAGES : pages;

        if (__builtin_wasm_memory_grow(0, pages) == (size_t)-1 && __builtin_wasm_memory_grow(0, (size - (uint32_t)(heap_end - heap_at) + PAGE - 1u) / PAGE) == (size_t)-1)
        {
            return 0;
        }

        heap_end = (uintptr_t)__builtin_wasm_memory_size(0) * PAGE;
    }

    at = heap_at;
    heap_at += size;

    return (void *)at;
}

void *web_alloc(uint32_t n)
{
    struct head *h;
    uint32_t class = 0u;

    if (n > 0x7FFFFFF0u - HEAD)
    {
        return 0;
    }

    while ((SMALLEST << class) < n + HEAD)
    {
        class++;
    }

    h = (struct head *)spares[class];

    if (h)
    {
        spares[class] = spares[class]->next;
    }

    else
    {
        h = carve(SMALLEST << class);
    }

    if (!h)
    {
        return 0;
    }

    h->class = class;

    return (unsigned char *)h + HEAD;
}

void web_free(void *p)
{
    struct head *h;
    struct spare *s;
    uint32_t class;

    if (!p)
    {
        return;
    }

    h = (struct head *)(void *)((unsigned char *)p - HEAD);
    class = h->class;
    s = (struct spare *)h;
    s->next = spares[class];
    spares[class] = s;
}

void *hl_mem_alloc(size_t n)
{
    return web_alloc((uint32_t)n);
}

void hl_mem_free(void *p)
{
    web_free(p);
}

void hl_panic(void)
{
    web_stop();
}

void *sess_mem_alloc(size_t n)
{
    return web_alloc((uint32_t)n);
}

void sess_mem_free(void *p)
{
    web_free(p);
}

void *ed_mem_alloc(size_t n)
{
    return web_alloc((uint32_t)n);
}

void ed_mem_free(void *p)
{
    web_free(p);
}

static uint32_t put(char *out, uint32_t k, uint32_t max, const char *s)
{
    while (*s && k + 1u < max)
    {
        out[k++] = *s++;
    }

    out[k] = 0;

    return k;
}

uint32_t ed_ask_close(const char *name)
{
    char text[512];
    uint32_t k = 0u;

    k = put(text, k, sizeof text, "\"");
    k = put(text, k, sizeof text, name);
    k = put(text, k, sizeof text, "\" ");
    k = put(text, k, sizeof text, ed_word(ED_W_ASK_TEXT));
    k = put(text, k, sizeof text, "\n\n");
    k = put(text, k, sizeof text, ed_word(ED_W_ASK_SAVE));
    put(text, k, sizeof text, "?");

    if (web_confirm(text))
    {
        return ED_CLOSE_SAVE;
    }

    k = put(text, 0u, sizeof text, ed_word(ED_W_ASK_DISCARD));
    put(text, k, sizeof text, "?");

    return web_confirm(text) ? ED_CLOSE_DISCARD : ED_CLOSE_CANCEL;
}

void ed_erase(void)
{
    if (web_confirm(ed_word(ED_W_ERASE_ASK)))
    {
        web_erase();
    }
}

uint32_t ed_tongue_count(void)
{
    return 1u;
}

const char *ed_tongue_tag(uint32_t i)
{
    return i ? "" : "zh-CN";
}

const char *ed_tongue_text(uint32_t i, uint32_t *n)
{
    *n = i ? 0u : web_tongue_zh_cn_size;

    return i ? 0 : (const char *)web_tongue_zh_cn;
}
