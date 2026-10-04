#include "sdl3.h"
#include "ed.h"

#define TONGUES 64u

struct tongue {
    char tag[ED_TAG_BYTES];
    char *path;
};

extern const unsigned char sdl3_tongue_zh_cn[];
extern const unsigned sdl3_tongue_zh_cn_size;
extern const unsigned char sdl3_tongue_template[];
extern const unsigned sdl3_tongue_template_size;

static const char readme[] =
    "Translations of TabPad's menus and messages: one text file each, named after the language.\n"
    "\n"
    "  de.txt    name = Deutsch\n"
    "            file.new = Neu\n"
    "            file.open = \xC3\x96" "ffnen...\n"
    "\n"
    "english.template lists every line there is, in English: copy it, name the copy after\n"
    "your language, end the name in .txt, and change what is to the right of each = sign.\n"
    "A line that is left out stays English. The file is UTF-8. A line starting with # is a note.\n"
    "TabPad reads this folder when it starts, and offers what it finds under\n"
    "View, Interface Language.\n";

static struct tongue tongues[TONGUES];
static uint32_t tongue_n;
static char *held;

static void add(const char *tag, const char *path)
{
    uint32_t i;

    for (i = 0u; i < tongue_n && SDL_strcasecmp(tongues[i].tag, tag); i++)
    {
    }

    if (i == TONGUES || SDL_strlen(tag) >= ED_TAG_BYTES)
    {
        return;
    }

    if (i < tongue_n)
    {
        SDL_free(tongues[i].path);
    }

    SDL_strlcpy(tongues[i].tag, tag, ED_TAG_BYTES);
    tongues[i].path = path ? SDL_strdup(path) : 0;
    tongue_n += i == tongue_n ? 1u : 0u;
}

static SDL_EnumerationResult entry(void *userdata, const char *dir, const char *name)
{
    char path[1200];
    char tag[64];
    size_t n = SDL_strlen(name);
    size_t d = SDL_strlen(dir);
    const char *gap = d && (dir[d - 1u] == '/' || dir[d - 1u] == '\\') ? "" : "/";

    (void)userdata;

    if (n < 5u || n - 4u >= sizeof tag || SDL_strcasecmp(name + n - 4u, ".txt") || !SDL_strcasecmp(name, "README.txt"))
    {
        return SDL_ENUM_CONTINUE;
    }

    SDL_strlcpy(tag, name, n - 3u);
    SDL_snprintf(path, sizeof path, "%s%s%s", dir, gap, name);
    add(tag, path);

    return SDL_ENUM_CONTINUE;
}

void sdl3_tongues(const char *dir)
{
    char path[1100];
    char file[1200];
    const char *base = SDL_GetBasePath();
    char *pref = dir ? 0 : SDL_GetPrefPath(0, "tabpad");

    add("zh-CN", 0);

    if (base)
    {
        SDL_snprintf(path, sizeof path, "%stranslations", base);
        SDL_EnumerateDirectory(path, entry, 0);
    }

    if (dir)
    {
        SDL_snprintf(path, sizeof path, "%s", dir);
    }

    else
    {
        SDL_snprintf(path, sizeof path, "%stranslations", pref ? pref : "");
    }

    SDL_free(pref);

    if (!SDL_GetPathInfo(path, 0) && SDL_CreateDirectory(path))
    {
        SDL_snprintf(file, sizeof file, "%s/README.txt", path);
        SDL_SaveFile(file, readme, sizeof readme - 1u);
        SDL_snprintf(file, sizeof file, "%s/english.template", path);
        SDL_SaveFile(file, sdl3_tongue_template, sdl3_tongue_template_size);
    }

    SDL_EnumerateDirectory(path, entry, 0);
}

uint32_t ed_tongue_count(void)
{
    return tongue_n;
}

const char *ed_tongue_tag(uint32_t i)
{
    return i < tongue_n ? tongues[i].tag : "";
}

const char *ed_tongue_text(uint32_t i, uint32_t *n)
{
    size_t size = 0u;

    *n = 0u;

    if (i >= tongue_n)
    {
        return 0;
    }

    if (!tongues[i].path)
    {
        *n = sdl3_tongue_zh_cn_size;

        return (const char *)sdl3_tongue_zh_cn;
    }

    SDL_free(held);
    held = SDL_LoadFile(tongues[i].path, &size);
    *n = held && size < 0x100000u ? (uint32_t)size : 0u;

    return held;
}
