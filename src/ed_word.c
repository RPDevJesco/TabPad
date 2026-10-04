#include "ed_int.h"

struct word {
    const char *key;
    const char *text;
};

static const struct word words[ED_W_COUNT] = {
    { "menu.file", "File" },
    { "menu.edit", "Edit" },
    { "menu.search", "Search" },
    { "menu.view", "View" },
    { "menu.encoding", "Encoding" },
    { "menu.language", "Language" },
    { "menu.window", "Window" },
    { "file.new", "New" },
    { "file.open", "Open..." },
    { "file.reload", "Reload from Disk" },
    { "file.save", "Save" },
    { "file.save_as", "Save As..." },
    { "file.save_all", "Save All" },
    { "file.close", "Close" },
    { "file.close_all", "Close All" },
    { "file.close_others", "Close All But This" },
    { "file.exit", "Exit" },
    { "edit.undo", "Undo" },
    { "edit.redo", "Redo" },
    { "edit.cut", "Cut" },
    { "edit.copy", "Copy" },
    { "edit.paste", "Paste" },
    { "edit.delete", "Delete" },
    { "edit.select_all", "Select All" },
    { "edit.duplicate_line", "Duplicate Line" },
    { "edit.delete_line", "Delete Line" },
    { "edit.line_up", "Move Line Up" },
    { "edit.line_down", "Move Line Down" },
    { "edit.indent", "Increase Line Indent" },
    { "edit.unindent", "Decrease Line Indent" },
    { "edit.upper", "UPPERCASE" },
    { "edit.lower", "lowercase" },
    { "edit.trim", "Trim Trailing Space" },
    { "edit.eol_crlf", "Line Ends: Windows (CR LF)" },
    { "edit.eol_lf", "Line Ends: Unix (LF)" },
    { "edit.eol_cr", "Line Ends: Macintosh (CR)" },
    { "edit.overwrite", "Overwrite Mode" },
    { "search.find", "Find..." },
    { "search.next", "Find Next" },
    { "search.previous", "Find Previous" },
    { "search.replace", "Replace..." },
    { "search.goto", "Go to Line..." },
    { "view.toolbar", "Toolbar" },
    { "view.status", "Status Bar" },
    { "view.numbers", "Line Numbers" },
    { "view.spaces", "Show Spaces and Tabs" },
    { "view.wrap", "Word Wrap" },
    { "view.zoom_in", "Zoom In" },
    { "view.zoom_out", "Zoom Out" },
    { "view.zoom_reset", "Restore Default Zoom" },
    { "view.font_text", "Text Font..." },
    { "view.font_ui", "Interface Font..." },
    { "view.language", "Interface Language..." },
    { "language.normal", "Normal Text" },
    { "window.next", "Next Tab" },
    { "window.previous", "Previous Tab" },
    { "find.find", "Find:" },
    { "find.replace_with", "Replace:" },
    { "find.goto", "Go to line:" },
    { "find.next", "Next" },
    { "find.previous", "Previous" },
    { "find.case", "Aa" },
    { "find.word", "Word" },
    { "find.regex", ".*" },
    { "find.replace", "Replace" },
    { "find.replace_all", "Replace All" },
    { "find.go", "Go" },
    { "find.not_found", "Not found" },
    { "find.wrapped", "Wrapped" },
    { "find.replaced", "replaced" },
    { "find.no_memory", "No memory" },
    { "find.bad_pattern", "Bad pattern" },
    { "status.line", "Ln:" },
    { "status.column", "Col:" },
    { "status.position", "Pos:" },
    { "status.selection", "Sel:" },
    { "status.length", "length:" },
    { "status.lines", "lines:" },
    { "status.insert", "INS" },
    { "status.overwrite", "OVR" },
    { "status.normal", "Normal text file" },
    { "status.conflict", "file changed on disk" },
    { "status.save_failed", "could not save" },
    { "status.missing", "not on disk yet" },
    { "status.eol_lf", "Unix (LF)" },
    { "status.eol_crlf", "Windows (CR LF)" },
    { "status.eol_cr", "Macintosh (CR)" },
    { "pick.font_text", "Text Font" },
    { "pick.font_ui", "Interface Font" },
    { "pick.language", "Interface Language" },
    { "pick.default", "System default" },
    { "pick.english", "English" },
    { "pick.filter", "Type to filter" },
    { "tab.new", "new" },
    { "ask.title", "Close tab" },
    { "ask.text", "has changes that are in no file." },
    { "ask.save", "Save" },
    { "ask.discard", "Don't save" },
    { "ask.cancel", "Cancel" },
    { "file.erase", "Erase Everything Kept Here..." },
    { "ask.erase", "Erase every tab, file copy and setting kept here? This cannot be undone." },
};

#define NAMES 128u

struct named {
    const char *tag;
    const char *text;
};

static const char *now[ED_W_COUNT];
static struct named names[NAMES];
static uint32_t name_n;
static char *held;

const char *ed_word(uint32_t id)
{
    if (id >= ED_W_COUNT)
    {
        return "";
    }

    return now[id] ? now[id] : words[id].text;
}

static int is_blank(char c)
{
    return c == ' ' || c == '\t' || c == '\r';
}

static int pair(const char *text, uint32_t n, uint32_t *at, uint32_t *key, uint32_t *key_n, uint32_t *value, uint32_t *value_n)
{
    uint32_t i = *at;
    uint32_t end;
    uint32_t eq;

    while (i < n)
    {
        for (end = i; end < n && text[end] != '\n'; end++)
        {
        }

        for (eq = i; eq < end && text[eq] != '='; eq++)
        {
        }

        *at = end < n ? end + 1u : n;

        if (!i && n >= 3u && (uint8_t)text[0] == 0xEFu && (uint8_t)text[1] == 0xBBu && (uint8_t)text[2] == 0xBFu)
        {
            i = 3u;
        }

        while (i < eq && is_blank(text[i]))
        {
            i++;
        }

        if (eq == end || i == eq || text[i] == '#')
        {
            i = *at;
            continue;
        }

        *key = i;

        for (*key_n = eq - i; *key_n && is_blank(text[i + *key_n - 1u]); (*key_n)--)
        {
        }

        for (eq++; eq < end && is_blank(text[eq]); eq++)
        {
        }

        *value = eq;

        for (*value_n = end - eq; *value_n && is_blank(text[eq + *value_n - 1u]); (*value_n)--)
        {
        }

        return 1;
    }

    return 0;
}

static int same(const char *s, uint32_t n, const char *lit)
{
    uint32_t i;

    for (i = 0u; i < n; i++)
    {
        if (!lit[i] || lit[i] != s[i])
        {
            return 0;
        }
    }

    return !lit[n];
}

void ed_word_load(const char *text, uint32_t n)
{
    uint32_t at = 0u;
    uint32_t key;
    uint32_t key_n;
    uint32_t value;
    uint32_t value_n;
    uint32_t i;

    for (i = 0u; i < ED_W_COUNT; i++)
    {
        now[i] = 0;
    }

    name_n = 0u;

    if (held)
    {
        ed_mem_free(held);
    }

    held = text && n ? ed_mem_alloc((size_t)n + 1u) : 0;

    if (!held)
    {
        return;
    }

    for (i = 0u; i < n; i++)
    {
        held[i] = text[i];
    }

    held[n] = 0;

    while (pair(held, n, &at, &key, &key_n, &value, &value_n))
    {
        for (i = 0u; i < ED_W_COUNT && !same(held + key, key_n, words[i].key); i++)
        {
        }

        if (!value_n)
        {
            continue;
        }

        held[value + value_n] = 0;
        held[key + key_n] = 0;

        if (i < ED_W_COUNT)
        {
            now[i] = held + value;
        }

        else if (key_n > 5u && same(held + key, 5u, "lang.") && name_n < NAMES)
        {
            names[name_n].tag = held + key + 5u;
            names[name_n++].text = held + value;
        }
    }
}

const char *ed_lang_name(uint32_t ext)
{
    uint32_t n = 0u;
    uint32_t i;

    while (ed_exts[ext].tag[n])
    {
        n++;
    }

    for (i = 0u; i < name_n; i++)
    {
        if (same(ed_exts[ext].tag, n, names[i].tag))
        {
            return names[i].text;
        }
    }

    return ed_exts[ext].name;
}

uint32_t ed_word_name(const char *text, uint32_t n, uint16_t *out, uint32_t max)
{
    uint32_t at = 0u;
    uint32_t key;
    uint32_t key_n;
    uint32_t value;
    uint32_t value_n;

    while (text && pair(text, n, &at, &key, &key_n, &value, &value_n))
    {
        if (same(text + key, key_n, "name"))
        {
            return ed_utf8_units(text + value, value_n, out, max);
        }
    }

    return 0u;
}

void ed_tongue_apply(void)
{
    const char *text;
    uint32_t n = 0u;
    uint32_t i;

    for (i = 0u; ed_opt.tongue[0] && i < ed_tongue_count(); i++)
    {
        for (n = 0u; ed_opt.tongue[n]; n++)
        {
        }

        if (same(ed_opt.tongue, n, ed_tongue_tag(i)))
        {
            text = ed_tongue_text(i, &n);
            ed_word_load(text, n);

            return;
        }
    }

    ed_word_load(0, 0u);
}
