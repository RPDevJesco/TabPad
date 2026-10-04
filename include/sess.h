#ifndef SESS_H
#define SESS_H

#include <stdint.h>
#include <stddef.h>
#include "sess_format.h"

#define SESS_NONE 0xFFFFFFFFu

struct sess_tab {
    char lang[SESS_LANG_BYTES];
    uint32_t encoding, eol;
    uint32_t caret, anchor;
    uint32_t scroll_row, scroll_col;
    uint64_t disk_size, disk_mtime;
    uint32_t disk_crc;
    uint32_t user;
};

void *sess_mem_alloc(size_t n);
void sess_mem_free(void *p);
int sess_put_open(const char *name);
int sess_put_write(const void *buf, uint32_t n);
int sess_put_close(void);
int sess_get_open(const char *name, uint32_t *size);
int sess_get_read(void *buf, uint32_t n);
void sess_get_close(void);
int sess_del(const char *name);
int sess_list(int first, char *name, uint32_t max);
uint32_t sess_buf_len(uint32_t tab);
const uint16_t *sess_buf_run(uint32_t tab, uint32_t off, uint32_t *n);
int sess_load(void);
int sess_flush(void);
uint32_t sess_count(void);
uint32_t sess_at(uint32_t pos);
uint32_t sess_active(void);
void sess_set_active(uint32_t pos);
int sess_tab_new(uint32_t pos, const char *path);
void sess_tab_close(uint32_t tab);
void sess_tab_move(uint32_t tab, uint32_t pos);
int sess_tab_get(uint32_t tab, struct sess_tab *out);
int sess_tab_set(uint32_t tab, const struct sess_tab *in);
const char *sess_tab_path(uint32_t tab);
int sess_tab_set_path(uint32_t tab, const char *path);
void sess_edited(uint32_t tab);
void sess_saved(uint32_t tab);
int sess_unsaved(uint32_t tab);
uint32_t sess_backup_len(uint32_t tab);
int sess_backup_read(uint32_t tab, uint16_t *out, uint32_t max);
uint32_t sess_crc(uint32_t crc, const void *buf, uint32_t n);

#endif
