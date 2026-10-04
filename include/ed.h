#ifndef ED_H
#define ED_H

#include <stdint.h>
#include <stddef.h>
#include "ed_words.h"

#define ED_PICK_OPEN 0u
#define ED_PICK_SAVE 1u
#define ED_CLOSE_CANCEL 0u
#define ED_CLOSE_DISCARD 1u
#define ED_CLOSE_SAVE 2u
#define ED_EOL_LF 0u
#define ED_EOL_CRLF 1u
#define ED_EOL_CR 2u
#define ED_C_BACK 0
#define ED_C_TEXT 1
#define ED_C_LINE 2
#define ED_C_SELECTION 3
#define ED_C_CARET 4
#define ED_C_GUTTER_BACK 5
#define ED_C_GUTTER_TEXT 6
#define ED_C_SCROLL 7
#define ED_C_BAR_BACK 8
#define ED_C_TAB_BACK 9
#define ED_C_TAB_TEXT 10
#define ED_C_TAB_ACTIVE_BACK 11
#define ED_C_TAB_ACTIVE_TEXT 12
#define ED_C_STATUS_BACK 13
#define ED_C_STATUS_TEXT 14
#define ED_C_STATUS_WARN 15
#define ED_C_MENU_BACK 16
#define ED_C_MENU_TEXT 17
#define ED_C_MENU_HOT 18
#define ED_C_DIM 19
#define ED_C_BORDER 20
#define ED_C_ACCENT 21
#define ED_C_FIELD_BACK 22
#define ED_C_COUNT 23
#define ED_I_NEW 0
#define ED_I_OPEN 1
#define ED_I_SAVE 2
#define ED_I_SAVE_ALL 3
#define ED_I_CLOSE 4
#define ED_I_CLOSE_ALL 5
#define ED_I_CUT 6
#define ED_I_COPY 7
#define ED_I_PASTE 8
#define ED_I_UNDO 9
#define ED_I_REDO 10
#define ED_I_FIND 11
#define ED_I_REPLACE 12
#define ED_I_ZOOM_IN 13
#define ED_I_ZOOM_OUT 14
#define ED_I_SPACES 15
#define ED_I_WRAP 16
#define ED_I_COUNT 17
#define ED_ICON 16
#define ED_KEY_LEFT 1u
#define ED_KEY_RIGHT 2u
#define ED_KEY_UP 3u
#define ED_KEY_DOWN 4u
#define ED_KEY_HOME 5u
#define ED_KEY_END 6u
#define ED_KEY_PAGE_UP 7u
#define ED_KEY_PAGE_DOWN 8u
#define ED_KEY_BACKSPACE 9u
#define ED_KEY_DELETE 10u
#define ED_KEY_ENTER 11u
#define ED_KEY_TAB 12u
#define ED_KEY_ESCAPE 13u
#define ED_KEY_INSERT 14u
#define ED_KEY_F3 15u
#define ED_MOD_SHIFT 1u
#define ED_MOD_CTRL 2u
#define ED_MOD_ALT 4u
#define ED_FONT_UI 0u
#define ED_FONT_TEXT 1u
#define ED_NAME_BYTES 64u
#define ED_TAG_BYTES 16u

struct ed_ext {
    const char *ext;
    const char *tag;
    const char *title;
    const char *name;
    uint32_t lang;
    uint32_t columns;
};

extern const uint32_t ed_colours[ED_C_COUNT];
extern const char *const ed_icons[ED_I_COUNT][ED_ICON];
extern const uint32_t ed_syntax[];
extern const uint32_t ed_columns[];
extern const uint32_t ed_column_count;
extern const uint32_t ed_eol_default;
extern const int ed_erase_offered;
extern uint32_t ed_ext_count;
extern struct ed_ext ed_exts[];

void ed_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb);
void ed_draw_text(int32_t x, int32_t y, const uint16_t *text, uint32_t n, uint32_t rgb);
int32_t ed_text_width(const uint16_t *text, uint32_t n);
int32_t ed_line_height(void);
void ed_text_font(uint32_t font, int32_t percent);
void ed_draw_clip(int32_t x, int32_t y, int32_t w, int32_t h);
void *ed_mem_alloc(size_t n);
void ed_mem_free(void *p);
int ed_file_stat(const char *path, uint64_t *size, uint64_t *mtime);
int ed_file_get(const char *path, void *buf, uint32_t n);
int ed_file_put(const char *path, const void *buf, uint32_t n);
int ed_clip_put(const char *utf8, uint32_t n);
const char *ed_clip_get(uint32_t *n);
void ed_pick(uint32_t what);
uint32_t ed_ask_close(const char *name);
void ed_title(const char *name, int unsaved);
void ed_exit(void);
void ed_erase(void);
uint32_t ed_font_count(void);
const char *ed_font_name(uint32_t i);
int ed_font_use(uint32_t font, const char *name);
uint32_t ed_tongue_count(void);
const char *ed_tongue_tag(uint32_t i);
const char *ed_tongue_text(uint32_t i, uint32_t *n);
const char *ed_word(uint32_t id);
int ed_init(int32_t w, int32_t h, int32_t scale);
void ed_resize(int32_t w, int32_t h, int32_t scale);
void ed_key(uint32_t key, uint32_t mods);
void ed_text(const char *utf8, uint32_t n);
void ed_mouse_down(int32_t x, int32_t y, uint32_t mods, uint32_t clicks);
void ed_mouse_move(int32_t x, int32_t y);
void ed_mouse_up(void);
void ed_wheel(int32_t rows, uint32_t mods);
void ed_tick(uint64_t now_ms);
void ed_focus_lost(void);
int ed_open(const char *path);
void ed_picked(uint32_t what, const char *path);
int ed_stale(void);
void ed_draw(void);

#endif
