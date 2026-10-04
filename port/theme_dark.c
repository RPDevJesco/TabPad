#include "ed.h"
#include "hl.h"

const uint32_t ed_colours[ED_C_COUNT] = {
    0x1E1F22u, 0xD4D4D4u, 0x26282Cu, 0x264F78u, 0xE8E8E8u, 0x1E1F22u, 0x6E7681u, 0x4A4D52u,
    0x151618u, 0x202225u, 0x9DA5B4u, 0x1E1F22u, 0xFFFFFFu,
    0x151618u, 0x9DA5B4u, 0xE5C07Bu,
    0x202225u, 0xD4D4D4u, 0x3A3D42u, 0x6E7681u, 0x3A3D42u, 0x7AC7A0u, 0x151618u,
};

const char *const hl_theme[] = {
    "comment", "string", "number", "keyword", "type", "function", "property", "label",
    "constant", "constant.builtin", "boolean", "variable.builtin", "enumMember",
    "escape", "string.escape", "string.special", "string.special.key", "string.special.symbol",
    "conditional", "repeat", "preproc", "class", "interface", "enum", "module", "constructor", "method", "decorator", "field", "parameter", "variable.parameter",
    "embedded", "none",
    "tag", "tag.error", "attribute", "punctuation.special", "text.title", "text.literal", "text.uri", "text.reference", "markup.heading", "markup.link", "markup.raw",
    "text.emphasis", "text.strong", "markup.italic", "markup.strong", "markup.list", "markup.quote", "variable.member", "character", "string.regexp", "string.special.url", "diff.plus", "diff.minus",
};

const uint32_t ed_syntax[] = {
    0x6A9955u, 0xCE9178u, 0xB5CEA8u, 0xC586C0u, 0x4EC9B0u, 0xDCDCAAu, 0x9CDCFEu, 0xC8C8C8u,
    0x4FC1FFu, 0x569CD6u, 0x569CD6u, 0x569CD6u, 0x4FC1FFu,
    0xD7BA7Du, 0xD7BA7Du, 0xD16969u, 0x9CDCFEu, 0x4FC1FFu,
    0xC586C0u, 0xC586C0u, 0xC586C0u, 0x4EC9B0u, 0x4EC9B0u, 0x4EC9B0u, 0x4EC9B0u, 0x4EC9B0u, 0xDCDCAAu, 0xDCDCAAu, 0x9CDCFEu, 0x9CDCFEu, 0x9CDCFEu,
    0xD4D4D4u, 0xD4D4D4u,
    0x569CD6u, 0xF44747u, 0x9CDCFEu, 0x569CD6u, 0x569CD6u, 0xCE9178u, 0x3794FFu, 0x9CDCFEu, 0x569CD6u, 0x3794FFu, 0xCE9178u,
    0xC586C0u, 0x569CD6u, 0xC586C0u, 0x569CD6u, 0x569CD6u, 0x6A9955u, 0x9CDCFEu, 0xCE9178u, 0xD16969u, 0x3794FFu, 0x7AC7A0u, 0xF44747u,
};

const uint32_t ed_columns[] = {
    0x9CDCFEu, 0xCE9178u, 0xB5CEA8u, 0xC586C0u, 0xDCDCAAu, 0x4EC9B0u, 0x569CD6u, 0xD7BA7Du,
};

const uint32_t ed_column_count = sizeof ed_columns / sizeof ed_columns[0];

const uint32_t hl_theme_count = sizeof hl_theme / sizeof hl_theme[0];
