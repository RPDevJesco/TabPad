#include "ed_int.h"
#include "sess_format.h"

#define CFG_NAME "cfg"
#define CFG_MAGIC 0
#define CFG_FLAGS 8
#define CFG_ZOOM 12
#define CFG_FONT_UI 16
#define CFG_FONT_TEXT 80
#define CFG_TONGUE 144
#define CFG_CRC 160
#define CFG_BYTES 164u
#define F_TOOL 1u
#define F_STATUS 2u
#define F_NUMBERS 4u
#define F_SPACES 8u
#define F_WRAP 16u
#define F_CASE 32u
#define F_WORD 64u
#define F_REGEX 128u

static uint8_t kept[CFG_BYTES];

static void put_text(uint8_t *to, const char *from, uint32_t max)
{
    uint32_t i;

    for (i = 0u; i < max; i++)
    {
        to[i] = i + 1u < max && *from ? (uint8_t)*from++ : 0u;
    }
}

static void get_text(char *to, const uint8_t *from, uint32_t max)
{
    uint32_t i;

    for (i = 0u; i < max; i++)
    {
        to[i] = (char)from[i];
    }

    to[max - 1u] = 0;
}

static void pack(uint8_t *b)
{
    const char *magic = "TABPADc1";
    uint32_t flags = 0u;
    uint32_t i;

    for (i = 0u; i < 8u; i++)
    {
        b[CFG_MAGIC + i] = (uint8_t)magic[i];
    }

    flags |= ed_opt.tool ? F_TOOL : 0u;
    flags |= ed_opt.status ? F_STATUS : 0u;
    flags |= ed_opt.numbers ? F_NUMBERS : 0u;
    flags |= ed_opt.spaces ? F_SPACES : 0u;
    flags |= ed_opt.wrap ? F_WRAP : 0u;
    flags |= ed_opt.find_case ? F_CASE : 0u;
    flags |= ed_opt.find_word ? F_WORD : 0u;
    flags |= ed_opt.find_regex ? F_REGEX : 0u;
    sess_st32(b + CFG_FLAGS, flags);
    sess_st32(b + CFG_ZOOM, (uint32_t)ed_opt.zoom);
    put_text(b + CFG_FONT_UI, ed_opt.font_ui, ED_NAME_BYTES);
    put_text(b + CFG_FONT_TEXT, ed_opt.font_text, ED_NAME_BYTES);
    put_text(b + CFG_TONGUE, ed_opt.tongue, ED_TAG_BYTES);
    sess_st32(b + CFG_CRC, sess_crc(0u, b, CFG_CRC));
}

static void unpack(const uint8_t *b)
{
    uint32_t flags = sess_ld32(b + CFG_FLAGS);
    uint32_t zoom = sess_ld32(b + CFG_ZOOM);

    ed_opt.tool = (flags & F_TOOL) != 0u;
    ed_opt.status = (flags & F_STATUS) != 0u;
    ed_opt.numbers = (flags & F_NUMBERS) != 0u;
    ed_opt.spaces = (flags & F_SPACES) != 0u;
    ed_opt.wrap = (flags & F_WRAP) != 0u;
    ed_opt.find_case = (flags & F_CASE) != 0u;
    ed_opt.find_word = (flags & F_WORD) != 0u;
    ed_opt.find_regex = (flags & F_REGEX) != 0u;
    ed_opt.zoom = zoom >= 50u && zoom <= 400u ? (int32_t)zoom : 100;
    get_text(ed_opt.font_ui, b + CFG_FONT_UI, ED_NAME_BYTES);
    get_text(ed_opt.font_text, b + CFG_FONT_TEXT, ED_NAME_BYTES);
    get_text(ed_opt.tongue, b + CFG_TONGUE, ED_TAG_BYTES);
}

void ed_cfg_load(void)
{
    uint8_t b[CFG_BYTES];
    uint32_t size = 0u;
    int got = 0;

    if (!sess_get_open(CFG_NAME, &size))
    {
        got = size == CFG_BYTES && !sess_get_read(b, CFG_BYTES);
        sess_get_close();
    }

    if (got && sess_magic_eq(b, "TABPADc1") && sess_ld32(b + CFG_CRC) == sess_crc(0u, b, CFG_CRC))
    {
        unpack(b);
    }

    pack(kept);

    if (ed_font_use(ED_FONT_UI, ed_opt.font_ui))
    {
        ed_font_use(ED_FONT_UI, "");
    }

    if (ed_font_use(ED_FONT_TEXT, ed_opt.font_text))
    {
        ed_font_use(ED_FONT_TEXT, "");
    }

    ed_tongue_apply();
}

void ed_cfg_save(void)
{
    uint8_t b[CFG_BYTES];
    uint32_t i;

    pack(b);

    for (i = 0u; i < CFG_BYTES && b[i] == kept[i]; i++)
    {
    }

    if (i == CFG_BYTES || sess_put_open(CFG_NAME))
    {
        return;
    }

    if (sess_put_write(b, CFG_BYTES))
    {
        sess_put_close();
        return;
    }

    if (sess_put_close())
    {
        return;
    }

    for (i = 0u; i < CFG_BYTES; i++)
    {
        kept[i] = b[i];
    }
}
