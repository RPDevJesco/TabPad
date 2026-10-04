#ifndef ED_INT_H
#define ED_INT_H

#include "ed.h"
#include "hl.h"
#include "sess.h"

#define ED_NONE 0xFFFFFFFFu
#define ED_ENC_UTF8 0u
#define ED_ENC_UTF8_BOM 1u
#define ED_ENC_UTF16_LE 2u
#define ED_ENC_UTF16_BE 3u
#define ED_ENC_LATIN1 4u
#define ED_CMD_NEW 1u
#define ED_CMD_OPEN 2u
#define ED_CMD_RELOAD 3u
#define ED_CMD_SAVE 4u
#define ED_CMD_SAVE_AS 5u
#define ED_CMD_SAVE_ALL 6u
#define ED_CMD_CLOSE 7u
#define ED_CMD_CLOSE_ALL 8u
#define ED_CMD_CLOSE_OTHERS 9u
#define ED_CMD_EXIT 10u
#define ED_CMD_ERASE 11u
#define ED_CMD_UNDO 20u
#define ED_CMD_REDO 21u
#define ED_CMD_CUT 22u
#define ED_CMD_COPY 23u
#define ED_CMD_PASTE 24u
#define ED_CMD_DELETE 25u
#define ED_CMD_SELECT_ALL 26u
#define ED_CMD_DUP_LINE 27u
#define ED_CMD_DEL_LINE 28u
#define ED_CMD_LINE_UP 29u
#define ED_CMD_LINE_DOWN 30u
#define ED_CMD_INDENT 31u
#define ED_CMD_UNINDENT 32u
#define ED_CMD_UPPER 33u
#define ED_CMD_LOWER 34u
#define ED_CMD_TRIM 35u
#define ED_CMD_EOL 36u
#define ED_CMD_OVERWRITE 37u
#define ED_CMD_FIND 40u
#define ED_CMD_FIND_NEXT 41u
#define ED_CMD_FIND_PREV 42u
#define ED_CMD_REPLACE 43u
#define ED_CMD_GOTO 44u
#define ED_CMD_TOOLBAR 50u
#define ED_CMD_STATUS 51u
#define ED_CMD_NUMBERS 52u
#define ED_CMD_SPACES 53u
#define ED_CMD_ZOOM_IN 54u
#define ED_CMD_ZOOM_OUT 55u
#define ED_CMD_ZOOM_RESET 56u
#define ED_CMD_WRAP 57u
#define ED_CMD_FONT_TEXT 80u
#define ED_CMD_FONT_UI 81u
#define ED_CMD_TONGUE 82u
#define ED_CMD_ENCODING 60u
#define ED_CMD_LANGUAGE 61u
#define ED_CMD_TAB_NEXT 70u
#define ED_CMD_TAB_PREV 71u
#define ED_CMD_TAB 72u
#define ED_PAD ed_px(6)
#define ED_HIT_NONE 0u
#define ED_HIT_TEXT 1u
#define ED_HIT_TAB 2u
#define ED_HIT_TAB_CLOSE 3u
#define ED_HIT_TAB_NEW 4u
#define ED_HIT_SCROLL 5u

struct ed_step {
    uint32_t at, removed, inserted;
    uint32_t caret_before, anchor_before, caret_after;
    uint16_t *text;
    uint32_t room;
};

struct ed_undo {
    struct ed_step *steps;
    uint32_t n, max, pos;
    int typing;
};

struct ed_step *ed_undo_push(struct ed_undo *u, uint32_t removed, uint32_t inserted);
struct ed_step *ed_undo_join(struct ed_undo *u, uint32_t at, uint16_t unit);

struct ed_tab {
    int loaded, conflict, missing, save_failed;
    uint32_t doc, lang, ext;
    uint32_t caret, anchor;
    int32_t want_x;
    uint32_t top, sub;
    int32_t left;
    uint32_t bytes, mark_off, mark_bytes;
    struct ed_undo undo;
};

extern uint32_t ed_cur;
extern struct ed_tab *ed_now;
struct ed_tab *ed_tab_of(uint32_t tab);

struct ed_opts {
    int tool, status, numbers, spaces, overwrite;
    int32_t zoom;
    int wrap, find_case, find_word, find_regex;
    char font_ui[ED_NAME_BYTES];
    char font_text[ED_NAME_BYTES];
    char tongue[ED_TAG_BYTES];
};

extern struct ed_opts ed_opt;

struct ed_bands {
    int32_t ui_lh, lh;
    int32_t icon, stroke;
    int32_t scroll_w;
    int32_t menu_h;
    int32_t tool_y, tool_h;
    int32_t bar_y, bar_h;
    int32_t text_y, text_h;
    int32_t panel_y, panel_h;
    int32_t status_y, status_h;
    int32_t gutter_w, text_x, text_w;
};

extern struct ed_bands ed_g;

extern int32_t ed_w;
extern int32_t ed_h;
extern int32_t ed_scale;
extern int ed_close_after_save;

int ed_decode(const uint8_t *bytes, uint32_t n, int detect, uint16_t **text, uint32_t *len, uint32_t *enc, uint32_t *eol);
int ed_encode(uint32_t doc, uint32_t from, uint32_t to, uint32_t enc, uint32_t eol, uint8_t **bytes, uint32_t *n);
uint32_t ed_utf8_units(const char *utf8, uint32_t n, uint16_t *out, uint32_t max);
void ed_undo_pop(struct ed_undo *u);
void ed_undo_free(struct ed_undo *u);
int ed_tab_new(uint32_t pos, const char *path);
void ed_tab_show(uint32_t pos);
int ed_tab_close(void);
void ed_tab_save(int as);
void ed_tab_save_all(void);
void ed_tab_saved_as(const char *path);
void ed_tab_reload(void);
void ed_tab_lang(uint32_t ext);
int ed_tabs_adopt(void);
uint32_t ed_tab_name(uint32_t tab, uint16_t *out, uint32_t max);
void ed_tab_sync(void);
void ed_tab_title(void);
int ed_replace(uint32_t at, uint32_t removed, const uint16_t *ins, uint32_t n, int typed);
void ed_edit_key(uint32_t key, uint32_t mods);
void ed_edit_text(const uint16_t *text, uint32_t n);
void ed_edit_run(uint32_t cmd);
void ed_go(uint32_t off, int keep);
uint32_t ed_sel_from(void);
uint32_t ed_sel_to(void);
void ed_select_word(uint32_t off);
void ed_select_row(uint32_t off);
uint16_t ed_unit(uint32_t off);
uint32_t ed_bytes_before(uint32_t off);
uint32_t ed_weigh(const uint16_t *text, uint32_t n);
void ed_run(uint32_t cmd, uint32_t arg);
int ed_can(uint32_t cmd, uint32_t arg);
int ed_on(uint32_t cmd, uint32_t arg);
int ed_key_cmd(uint32_t key, uint32_t mods, uint32_t *cmd, uint32_t *arg);
const char *ed_cmd_key(uint32_t cmd, uint32_t arg);
void ed_measure(void);
int32_t ed_say(int32_t x, int32_t y, const char *utf8, uint32_t rgb);
int32_t ed_say_width(const char *utf8);
uint32_t ed_hit(int32_t x, int32_t y, uint32_t *pos);
uint32_t ed_off_at(int32_t x, int32_t y);
uint32_t ed_off_lines(uint32_t off, int32_t by, int32_t x);
uint32_t ed_line_start(uint32_t off);
uint32_t ed_line_end(uint32_t off);
void ed_scroll_lines(int32_t by);
int32_t ed_px(int32_t n);
int32_t ed_x_of(uint32_t off);
uint32_t ed_col_of(uint32_t off);
uint32_t ed_rows_shown(void);
void ed_reveal(void);
void ed_scroll_to(int32_t y);
void ed_view_draw(void);
int ed_menu_open(void);
int ed_menu_down(int32_t x, int32_t y);
int ed_menu_move(int32_t x, int32_t y);
int ed_menu_key(uint32_t key, uint32_t mods);
void ed_menu_draw(void);
void ed_menu_draw_open(void);
int ed_tool_down(int32_t x, int32_t y);
void ed_tool_move(int32_t x, int32_t y);
void ed_tool_draw(void);
void ed_find_open(uint32_t kind);
void ed_find_close(void);
int32_t ed_find_height(int32_t ui_lh);
int ed_find_focus(void);
void ed_find_blur(void);
int ed_find_ready(void);
void ed_find_next(int back);
int ed_find_key(uint32_t key, uint32_t mods);
int ed_find_down(int32_t x, int32_t y);
void ed_find_text(const uint16_t *text, uint32_t n);
void ed_find_draw(void);
void ed_status_draw(void);
void ed_touch(void);
void ed_cfg_load(void);
void ed_cfg_save(void);
void ed_word_load(const char *text, uint32_t n);
uint32_t ed_word_name(const char *text, uint32_t n, uint16_t *out, uint32_t max);
void ed_tongue_apply(void);
const char *ed_lang_name(uint32_t ext);
uint32_t ed_units_utf8(const uint16_t *text, uint32_t n, char *out, uint32_t max);
void ed_list_open(uint32_t what);
int ed_list_shown(void);
void ed_list_key(uint32_t key);
void ed_list_text(const uint16_t *text, uint32_t n);
void ed_list_down(int32_t x, int32_t y);
void ed_list_move(int32_t x, int32_t y);
void ed_list_wheel(int32_t rows);
void ed_list_draw(void);

#endif
