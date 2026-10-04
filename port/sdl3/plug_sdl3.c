#include "sdl3.h"
#include "ed.h"
#include "hl.h"
#include "langs.h"

#define PATH_BYTES 2048u
#define FOLDERS 256u
#define TAG_BYTES 16u
#define LABEL_BYTES 48u

#ifdef SDL_PLATFORM_WINDOWS
#define LIBRARY ".dll"
#elif defined(SDL_PLATFORM_APPLE)
#define LIBRARY ".dylib"
#else
#define LIBRARY ".so"
#endif

struct manifest {
    char title[LABEL_BYTES];
    char name[LABEL_BYTES];
    char symbol[LABEL_BYTES];
    char endings[512];
};

static SDL_IOStream *log_io;

static void say(const char *tag, const char *what)
{
    char line[512];

    SDL_snprintf(line, sizeof line, "%s: %s\n", tag, what);
    SDL_Log("tabpad: language %s", line);

    if (log_io)
    {
        SDL_WriteIO(log_io, line, SDL_strlen(line));
    }
}

static int tag_ok(const char *tag)
{
    size_t i;

    for (i = 0u; tag[i]; i++)
    {
        if (!((tag[i] >= 'a' && tag[i] <= 'z') || (tag[i] >= '0' && tag[i] <= '9') || tag[i] == '_' || tag[i] == '-'))
        {
            return 0;
        }
    }

    return i > 0u && i < TAG_BYTES;
}

static uint32_t row_of(const char *tag)
{
    uint32_t i;

    for (i = 0u; i < ed_ext_count && SDL_strcmp(ed_exts[i].tag, tag); i++)
    {
    }

    return i;
}

static void take(char *out, size_t max, const char *from, const char *to)
{
    size_t n;

    while (from < to && (*from == ' ' || *from == '\t'))
    {
        from++;
    }

    while (to > from && (to[-1] == ' ' || to[-1] == '\t' || to[-1] == '\r'))
    {
        to--;
    }

    n = (size_t)(to - from) < max - 1u ? (size_t)(to - from) : max - 1u;
    SDL_memcpy(out, from, n);
    out[n] = 0;
}

static void read_manifest(const char *folder, struct manifest *m)
{
    char path[PATH_BYTES];
    char key[32];
    char *text;
    const char *line;
    const char *end;
    const char *eq;

    SDL_zerop(m);
    SDL_snprintf(path, sizeof path, "%s/language.ini", folder);
    text = SDL_LoadFile(path, 0);

    for (line = text; line && *line; line = *end ? end + 1 : end)
    {
        end = SDL_strchr(line, '\n');
        end = end ? end : line + SDL_strlen(line);
        eq = SDL_strchr(line, '=');

        if (!eq || eq > end || *line == '#')
        {
            continue;
        }

        take(key, sizeof key, line, eq);

        if (!SDL_strcmp(key, "title"))
        {
            take(m->title, sizeof m->title, eq + 1, end);
        }

        else if (!SDL_strcmp(key, "name"))
        {
            take(m->name, sizeof m->name, eq + 1, end);
        }

        else if (!SDL_strcmp(key, "symbol"))
        {
            take(m->symbol, sizeof m->symbol, eq + 1, end);
        }

        else if (!SDL_strcmp(key, "endings"))
        {
            take(m->endings, sizeof m->endings, eq + 1, end);
        }
    }

    SDL_free(text);
}

static SDL_FunctionPointer load_grammar(const char *folder, const char *tag, const char *symbol, int *bad)
{
    char path[PATH_BYTES];
    char entry[96];
    SDL_SharedObject *lib;
    SDL_FunctionPointer f;
    size_t i;

    SDL_snprintf(path, sizeof path, "%s/%s" LIBRARY, folder, tag);

    if (!SDL_GetPathInfo(path, 0))
    {
        return 0;
    }

    *bad = 1;
    lib = SDL_LoadObject(path);

    if (!lib)
    {
        say(tag, "left out: the system would not load " LIBRARY " of it");

        return 0;
    }

    SDL_snprintf(entry, sizeof entry, "tree_sitter_%s", symbol[0] ? symbol : tag);

    for (i = 0u; entry[i]; i++)
    {
        entry[i] = entry[i] == '-' ? '_' : entry[i];
    }

    f = SDL_LoadFunction(lib, entry);

    if (!f)
    {
        say(tag, "left out: its library has no tree_sitter_<symbol> for it");

        return 0;
    }

    *bad = 0;

    return f;
}

static void give_ending(const char *ext, const struct ed_ext *as)
{
    uint32_t i;

    for (i = 0u; i < ed_ext_count && SDL_strcmp(ed_exts[i].ext, ext); i++)
    {
    }

    if (i == ed_ext_count)
    {
        if (ed_ext_count == LANGS_EXT_MAX - 1u)
        {
            return;
        }

        ed_exts[i].ext = SDL_strdup(ext);
        ed_ext_count += ed_exts[i].ext ? 1u : 0u;
    }

    ed_exts[i].tag = as->tag;
    ed_exts[i].title = as->title;
    ed_exts[i].name = as->name;
    ed_exts[i].lang = as->lang;
    ed_exts[i].columns = 0u;
}

static void load(const char *dir, const char *tag)
{
    char folder[PATH_BYTES];
    char path[PATH_BYTES];
    char ext[32];
    struct manifest m;
    struct ed_ext as;
    SDL_FunctionPointer grammar;
    char *query;
    const char *at;
    const char *to;
    uint32_t old = row_of(tag);
    uint32_t i;
    uint32_t given = 0u;
    int over = old < ed_ext_count;
    int bad = 0;

    if (!tag_ok(tag))
    {
        say(tag, "left out: a folder is named in a-z, 0-9, - and _, 15 at most");
        return;
    }

    SDL_snprintf(folder, sizeof folder, "%s/%s", dir, tag);
    read_manifest(folder, &m);
    grammar = load_grammar(folder, tag, m.symbol, &bad);

    if (bad)
    {
        return;
    }

    SDL_snprintf(path, sizeof path, "%s/highlights.scm", folder);
    query = SDL_LoadFile(path, 0);

    if ((!over && (!grammar || !query || !m.endings[0])) || (over && ed_exts[old].lang >= hl_lang_count && (!grammar || !query)))
    {
        say(tag, !grammar ? "left out: no " LIBRARY " in it" : !query ? "left out: no highlights.scm in it" : "left out: language.ini names no endings");
        SDL_free(query);
        return;
    }

    if (!grammar && !query)
    {
        say(tag, "left out: neither a library nor a highlights.scm in it");
        return;
    }

    if (hl_lang_count == LANGS_MAX)
    {
        say(tag, "left out: there is no room for another language");
        SDL_free(query);
        return;
    }

    hl_langs[hl_lang_count].grammar = grammar ? (const struct TSLanguage *(*)(void))grammar : hl_langs[ed_exts[old].lang].grammar;
    hl_langs[hl_lang_count].query = query ? query : hl_langs[ed_exts[old].lang].query;
    hl_langs[hl_lang_count].inner = grammar ? 0 : hl_langs[ed_exts[old].lang].inner;
    hl_langs[hl_lang_count].inner_count = grammar ? 0u : hl_langs[ed_exts[old].lang].inner_count;
    hl_langs[hl_lang_count].names = 0;
    hl_lang_count++;

    if (!hl_lang_ok(hl_lang_count - 1u))
    {
        say(tag, "left out: its grammar is of a version this program does not take, or its highlights.scm asks for something it cannot decide");
        return;
    }

    as.ext = 0;
    as.tag = over ? ed_exts[old].tag : SDL_strdup(tag);
    as.title = m.title[0] ? SDL_strdup(m.title) : over ? ed_exts[old].title : as.tag;
    as.name = m.name[0] ? SDL_strdup(m.name) : over ? ed_exts[old].name : as.title;
    as.lang = hl_lang_count - 1u;
    hl_langs[as.lang].names = as.tag;

    if (over && ed_exts[old].lang < hl_lang_count)
    {
        hl_langs[as.lang].names = hl_langs[ed_exts[old].lang].names;
        hl_langs[ed_exts[old].lang].names = 0;
    }

    if (!as.tag || !as.title || !as.name)
    {
        say(tag, "left out: no memory");
        return;
    }

    for (i = 0u; i < ed_ext_count; i++)
    {
        if (!SDL_strcmp(ed_exts[i].tag, tag))
        {
            give_ending(ed_exts[i].ext, &as);
        }
    }

    for (at = m.endings; *at; at = to)
    {
        while (*at == ' ' || *at == '\t' || *at == ',')
        {
            at++;
        }

        for (to = at; *to && *to != ' ' && *to != '\t' && *to != ','; to++)
        {
        }

        if (to == at || *at != '.' || (size_t)(to - at) >= sizeof ext)
        {
            continue;
        }

        take(ext, sizeof ext, at, to);

        for (i = 0u; ext[i]; i++)
        {
            ext[i] = (char)SDL_tolower((unsigned char)ext[i]);
        }

        give_ending(ext, &as);
        given++;
    }

    SDL_snprintf(path, sizeof path, "%s: %s%s, %u ending%s of its own", as.title, over ? "took over" : "added", !grammar ? " with the grammar that was there" : !query ? " with the query that was there" : "", (unsigned)given, given == 1u ? "" : "s");
    say(tag, path);
}

static SDL_EnumerationResult SDLCALL note_folder(void *names, const char *dir, const char *name)
{
    char path[PATH_BYTES];
    char **list = names;
    SDL_PathInfo info;
    uint32_t n;

    SDL_snprintf(path, sizeof path, "%s%s", dir, name);

    for (n = 0u; list[n]; n++)
    {
    }

    if (n < FOLDERS - 1u && SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY)
    {
        list[n] = SDL_strdup(name);
    }

    return SDL_ENUM_CONTINUE;
}

static int SDLCALL by_name(const void *a, const void *b)
{
    return SDL_strcmp(*(char *const *)a, *(char *const *)b);
}

void sdl3_languages(const char *dir, SDL_IOStream *log)
{
    static char *names[FOLDERS];
    uint32_t n;
    uint32_t i;

    log_io = log;
    SDL_memset(names, 0, sizeof names);

    if (!SDL_EnumerateDirectory(dir, note_folder, names))
    {
        return;
    }

    for (n = 0u; names[n]; n++)
    {
    }

    SDL_qsort(names, n, sizeof names[0], by_name);

    for (i = 0u; i < n; i++)
    {
        load(dir, names[i]);
        SDL_free(names[i]);
    }
}
