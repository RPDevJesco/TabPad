#ifndef HL_H
#define HL_H

#include <stdint.h>
#include <stddef.h>

#define HL_SPAN_AT 0
#define HL_SPAN_LEN 1
#define HL_SPAN_ID 2
#define HL_SPAN_WORDS 3
#define HL_LEN_MAX 0x7FFFFFF0u
#define HL_PLAIN 0xFFFFFFFFu

struct TSLanguage;

#define HL_NAMED 0xFFFFFFFEu
#define HL_INNER_MAX 4u
#define HL_GROUPS 10u

struct hl_inner {
    const char *node;
    const char *parent;
    uint32_t lang;
    int joined;
    const char *query;
};

struct hl_lang {
    const struct TSLanguage *(*grammar)(void);
    const char *query;
    const struct hl_inner *inner;
    uint32_t inner_count;
    const char *names;
};

extern struct hl_lang hl_langs[];
extern uint32_t hl_lang_count;
extern const char *const hl_theme[];
extern const uint32_t hl_theme_count;

void *hl_mem_alloc(size_t n);
void hl_mem_free(void *p);
void hl_panic(void);
int hl_lang_ok(uint32_t lang);
int hl_open(uint32_t lang, const uint16_t *text, uint32_t len);
void hl_close(uint32_t doc);
int hl_edit(uint32_t doc, uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n);
uint32_t hl_spans(uint32_t doc, uint32_t from, uint32_t to, uint32_t *out, uint32_t max);
int hl_dirty(uint32_t doc, uint32_t *from, uint32_t *to);
uint32_t hl_len(uint32_t doc);
const uint16_t *hl_text(uint32_t doc, uint32_t off, uint32_t *n);
int hl_row_col(uint32_t doc, uint32_t off, uint32_t *row, uint32_t *col);
uint32_t hl_rows(uint32_t doc);
int hl_row_start(uint32_t doc, uint32_t row, uint32_t *off);
uint32_t hl_settle(void);
int hl_find(uint32_t doc, const char *regex, uint32_t n, uint32_t from, uint32_t to, uint32_t *at, uint32_t *len, uint32_t *groups);

#endif
