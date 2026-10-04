#include "sdl3.h"
#include "desk.h"
#include "ed.h"
#include "stb_truetype.h"

#define ATLAS 1024
#define SLOTS 4096u
#define FACES 16u
#define DEPTH 6
#define NAME_TABLE 65536u
#define TTC_FONTS 32u
#define FULL_SEARCHES 2u
#define NONE 0xFFFFFFFFu

struct glyph {
    uint32_t key;
    int16_t x, y, w, h;
    int16_t dx, dy;
    int32_t advance;
};

struct found {
    char name[ED_NAME_BYTES];
    char *path;
    int index;
};

struct face {
    stbtt_fontinfo info;
    void *file;
    char *path;
    int index;
};

struct look {
    uint32_t face;
    uint32_t px;
    float scale;
    int32_t line_height, baseline;
};

extern const unsigned char sdl3_font_data[];

static const char *const ui_names[] = { "Segoe UI", "Noto Sans", "DejaVu Sans", "Cantarell", "Ubuntu", "Liberation Sans", "Helvetica Neue", "Lucida Grande", "Helvetica", "Arial", "Verdana" };
static const char *const text_names[] = { "Consolas", "Cascadia Mono", "DejaVu Sans Mono", "Liberation Mono", "Noto Sans Mono", "Ubuntu Mono", "Menlo", "Monaco", "Courier New" };
static const char *const wide_names[] = {
    "Microsoft YaHei", "Microsoft YaHei UI", "Yu Gothic", "Yu Gothic UI", "Malgun Gothic", "Microsoft JhengHei", "SimSun", "MS Gothic",
    "Noto Sans CJK SC", "Noto Sans CJK JP", "Noto Sans CJK KR", "Noto Sans CJK TC", "Noto Sans SC", "Noto Sans JP", "Noto Sans KR", "Source Han Sans SC", "Source Han Sans CN",
    "WenQuanYi Micro Hei", "WenQuanYi Zen Hei", "Droid Sans Fallback", "PingFang SC", "Hiragino Sans", "Hiragino Sans GB", "Apple SD Gothic Neo", "Arial Unicode MS",
    "Segoe UI Symbol", "Segoe UI Historic", "Noto Sans Symbols", "Noto Sans Symbols2", "Noto Sans Arabic", "Noto Sans Hebrew", "Noto Sans Devanagari", "Noto Sans Thai", "DejaVu Sans", "Symbola",
};

static struct found *founds;
static uint32_t found_n;
static uint32_t found_max;
static int scanned;
static struct face faces[FACES];
static uint32_t face_n;
static uint32_t chosen[2];
static uint32_t defaults[2] = { NONE, NONE };
static uint32_t forced = NONE;
static struct look looks[2];
static struct look *cur = &looks[0];
static uint32_t cur_font;
static float base_px;
static uint32_t searches;
static SDL_Texture *atlas;
static struct glyph glyphs[SLOTS];
static uint32_t glyph_count;
static int32_t shelf_x, shelf_y, shelf_h;

static uint32_t be16(const uint8_t *p)
{
    return (uint32_t)p[0] << 8 | p[1];
}

static uint32_t be32(const uint8_t *p)
{
    return be16(p) << 16 | be16(p + 2);
}

static int grab(SDL_IOStream *io, uint32_t at, void *out, uint32_t n)
{
    return SDL_SeekIO(io, (Sint64)at, SDL_IO_SEEK_SET) == (Sint64)at && SDL_ReadIO(io, out, n) == n;
}

static uint32_t put_utf8(char *out, uint32_t k, uint32_t max, uint32_t c)
{
    if (c < 0x80u && k + 1u < max)
    {
        out[k++] = (char)c;
    }

    else if (c >= 0x80u && c < 0x800u && k + 2u < max)
    {
        out[k++] = (char)(0xC0u | c >> 6);
        out[k++] = (char)(0x80u | (c & 0x3Fu));
    }

    else if (c >= 0x800u && k + 3u < max)
    {
        out[k++] = (char)(0xE0u | c >> 12);
        out[k++] = (char)(0x80u | (c >> 6 & 0x3Fu));
        out[k++] = (char)(0x80u | (c & 0x3Fu));
    }

    return k;
}

static int name_of(const uint8_t *table, uint32_t size, uint32_t id, char *out, uint32_t max)
{
    uint32_t count = be16(table + 2);
    uint32_t strings = be16(table + 4);
    uint32_t best = NONE;
    uint32_t rank = 0u;
    uint32_t platform;
    uint32_t language;
    uint32_t score;
    uint32_t at;
    uint32_t n;
    uint32_t k = 0u;
    uint32_t i;
    const uint8_t *r;

    for (i = 0u; i < count && 6u + (i + 1u) * 12u <= size; i++)
    {
        r = table + 6u + i * 12u;
        platform = be16(r);
        language = be16(r + 4);

        if (be16(r + 6) != id)
        {
            continue;
        }

        score = platform == 3u ? (language == 0x409u ? 4u : 2u) : platform == 0u ? 3u : platform == 1u && !language ? 1u : 0u;

        if (score > rank)
        {
            rank = score;
            best = i;
        }
    }

    if (best == NONE)
    {
        return 0;
    }

    r = table + 6u + best * 12u;
    n = be16(r + 8);
    at = strings + be16(r + 10);

    if (at + n > size)
    {
        return 0;
    }

    for (i = 0u; i < n; i += be16(r) == 1u ? 1u : 2u)
    {
        k = put_utf8(out, k, max, be16(r) == 1u ? table[at + i] : be16(table + at + i));
    }

    out[k] = 0;

    return k > 0u;
}

static int regular(const char *style)
{
    return !style[0] || !SDL_strcasecmp(style, "Regular") || !SDL_strcasecmp(style, "Book") || !SDL_strcasecmp(style, "Roman") || !SDL_strcasecmp(style, "Normal");
}

static void keep(const char *name, const char *path, int index)
{
    struct found *grown;
    uint32_t i;

    for (i = 0u; i < found_n; i++)
    {
        if (!SDL_strcasecmp(founds[i].name, name))
        {
            return;
        }
    }

    if (found_n == found_max)
    {
        grown = SDL_realloc(founds, (found_max ? found_max * 2u : 256u) * sizeof *grown);

        if (!grown)
        {
            return;
        }

        founds = grown;
        found_max = found_max ? found_max * 2u : 256u;
    }

    SDL_strlcpy(founds[found_n].name, name, ED_NAME_BYTES);
    founds[found_n].path = SDL_strdup(path);
    founds[found_n].index = index;
    found_n += founds[found_n].path ? 1u : 0u;
}

static void probe_one(SDL_IOStream *io, uint32_t base, uint8_t *table, const char *path, int index)
{
    uint8_t head[12];
    uint8_t rec[16];
    char name[ED_NAME_BYTES];
    char style[ED_NAME_BYTES];
    uint32_t tables;
    uint32_t at = 0u;
    uint32_t size = 0u;
    uint32_t i;
    int outlines = 0;

    if (!grab(io, base, head, 12u))
    {
        return;
    }

    tables = be16(head + 4);

    for (i = 0u; i < tables && i < 128u && grab(io, base + 12u + i * 16u, rec, 16u); i++)
    {
        outlines |= !SDL_memcmp(rec, "glyf", 4u) || !SDL_memcmp(rec, "CFF ", 4u);

        if (!SDL_memcmp(rec, "name", 4u))
        {
            at = be32(rec + 8);
            size = be32(rec + 12);
        }
    }

    size = size > NAME_TABLE ? NAME_TABLE : size;
    style[0] = 0;

    if (!outlines || size < 6u || !grab(io, at, table, size) || !name_of(table, size, 1u, name, ED_NAME_BYTES))
    {
        return;
    }

    name_of(table, size, 2u, style, ED_NAME_BYTES);

    if (regular(style) && name[0] != '.')
    {
        keep(name, path, index);
    }
}

static void probe(const char *path)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    uint8_t *table = SDL_malloc(NAME_TABLE);
    uint8_t head[12];
    uint8_t off[4];
    uint32_t count;
    uint32_t i;

    if (io && table && grab(io, 0u, head, 12u))
    {
        if (SDL_memcmp(head, "ttcf", 4u))
        {
            probe_one(io, 0u, table, path, 0);
        }

        else
        {
            count = be32(head + 8);

            for (i = 0u; i < count && i < TTC_FONTS && grab(io, 12u + i * 4u, off, 4u); i++)
            {
                probe_one(io, be32(off), table, path, (int)i);
            }
        }
    }

    SDL_free(table);

    if (io)
    {
        SDL_CloseIO(io);
    }
}

static int is_font(const char *name)
{
    const char *dot = SDL_strrchr(name, '.');

    return dot && (!SDL_strcasecmp(dot, ".ttf") || !SDL_strcasecmp(dot, ".otf") || !SDL_strcasecmp(dot, ".ttc") || !SDL_strcasecmp(dot, ".otc"));
}

static void scan_dir(const char *dir, int depth);

static SDL_EnumerationResult scan_entry(void *userdata, const char *dir, const char *name)
{
    char path[1024];
    SDL_PathInfo info;
    size_t n = SDL_strlen(dir);
    const char *gap = n && (dir[n - 1u] == '/' || dir[n - 1u] == '\\') ? "" : "/";

    if (name[0] == '.' || SDL_snprintf(path, sizeof path, "%s%s%s", dir, gap, name) >= (int)sizeof path || !SDL_GetPathInfo(path, &info))
    {
        return SDL_ENUM_CONTINUE;
    }

    if (info.type == SDL_PATHTYPE_DIRECTORY)
    {
        scan_dir(path, *(int *)userdata + 1);
    }

    else if (is_font(name))
    {
        probe(path);
    }

    return SDL_ENUM_CONTINUE;
}

static void scan_dir(const char *dir, int depth)
{
    if (depth < DEPTH)
    {
        SDL_EnumerateDirectory(dir, scan_entry, &depth);
    }
}

static void scan_under(const char *root, const char *rest)
{
    char path[1024];

    if (root && root[0] && SDL_snprintf(path, sizeof path, "%s%s", root, rest) < (int)sizeof path)
    {
        scan_dir(path, 0);
    }
}

static int by_name(const void *a, const void *b)
{
    return SDL_strcasecmp(((const struct found *)a)->name, ((const struct found *)b)->name);
}

static void scan(void)
{
    const char *home = SDL_getenv("HOME");

    if (scanned)
    {
        return;
    }

    scanned = 1;
    scan_under(SDL_getenv("WINDIR"), "\\Fonts");
    scan_under(SDL_getenv("LOCALAPPDATA"), "\\Microsoft\\Windows\\Fonts");
    scan_under(SDL_getenv("XDG_DATA_HOME"), "/fonts");
    scan_under(home, "/.local/share/fonts");
    scan_under(home, "/.fonts");
    scan_under(home, "/Library/Fonts");
    scan_under("/usr/local/share/fonts", "");
    scan_under("/usr/share/fonts", "");
    scan_under("/Library/Fonts", "");
    scan_under("/System/Library/Fonts", "");

    if (found_n)
    {
        SDL_qsort(founds, found_n, sizeof *founds, by_name);
    }
}

static uint32_t found_of(const char *name)
{
    uint32_t i;

    scan();

    for (i = 0u; i < found_n; i++)
    {
        if (!SDL_strcasecmp(founds[i].name, name))
        {
            return i;
        }
    }

    return NONE;
}

static uint32_t face_load(const char *path, int index)
{
    void *file;
    uint32_t i;
    int off;

    for (i = 1u; i < face_n; i++)
    {
        if (faces[i].index == index && !SDL_strcmp(faces[i].path, path))
        {
            return i;
        }
    }

    if (face_n == FACES)
    {
        return NONE;
    }

    file = SDL_LoadFile(path, 0);
    off = file ? stbtt_GetFontOffsetForIndex(file, index) : -1;
    faces[face_n].path = SDL_strdup(path);

    if (off < 0 || !faces[face_n].path || !stbtt_InitFont(&faces[face_n].info, file, off))
    {
        SDL_free(faces[face_n].path);
        SDL_free(file);

        return NONE;
    }

    faces[face_n].file = file;
    faces[face_n].index = index;

    return face_n++;
}

static void face_drop(void)
{
    face_n--;
    SDL_free(faces[face_n].path);
    SDL_free(faces[face_n].file);
}

static uint32_t face_named(const char *name)
{
    uint32_t i = found_of(name);

    return i == NONE ? NONE : face_load(founds[i].path, founds[i].index);
}

static uint32_t face_default(uint32_t font)
{
    const char *const *names = font == ED_FONT_UI ? ui_names : text_names;
    uint32_t count = font == ED_FONT_UI ? (uint32_t)SDL_arraysize(ui_names) : (uint32_t)SDL_arraysize(text_names);
    char name[ED_NAME_BYTES];
    char path[1024];
    uint32_t face = NONE;
    uint32_t i;
    int index = 0;

    if (defaults[font] != NONE)
    {
        return defaults[font];
    }

    if (!desk_font(font == ED_FONT_UI ? DESK_FONT_UI : DESK_FONT_TEXT, name, sizeof name, path, sizeof path, &index))
    {
        face = path[0] ? face_load(path, index) : NONE;
        face = face == NONE && name[0] ? face_named(name) : face;
    }

    for (i = 0u; face == NONE && i < count; i++)
    {
        face = face_named(names[i]);
    }

    defaults[font] = face == NONE ? 0u : face;

    return defaults[font];
}

static void forget(void)
{
    SDL_memset(glyphs, 0, sizeof glyphs);
    glyph_count = 0u;
    shelf_x = 0;
    shelf_y = 0;
    shelf_h = 0;
}

static void measure(struct look *l)
{
    const stbtt_fontinfo *info = &faces[l->face].info;
    int ascent;
    int descent;
    int gap;

    stbtt_GetFontVMetrics(info, &ascent, &descent, &gap);
    l->scale = stbtt_ScaleForPixelHeight(info, (float)l->px);
    l->line_height = (int32_t)SDL_ceilf((float)(ascent - descent + gap) * l->scale);
    l->baseline = (int32_t)SDL_roundf((float)ascent * l->scale);
}

void sdl3_draw_open(const char *path, float px)
{
    atlas = SDL_CreateTexture(sdl3_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, ATLAS, ATLAS);
    SDL_SetTextureBlendMode(atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(atlas, SDL_SCALEMODE_NEAREST);
    stbtt_InitFont(&faces[0].info, sdl3_font_data, 0);
    face_n = 1u;
    base_px = px;
    forced = path && SDL_strcmp(path, "builtin") ? face_load(path, 0) : path ? 0u : NONE;

    if (path && forced == NONE)
    {
        SDL_Log("tabpad: cannot use the font %s", path);
    }

    chosen[ED_FONT_UI] = forced == NONE ? face_default(ED_FONT_UI) : forced;
    chosen[ED_FONT_TEXT] = forced == NONE ? face_default(ED_FONT_TEXT) : forced;
    looks[0].px = 0u;
    looks[1].px = 0u;
    ed_text_font(ED_FONT_UI, 100);
}

void sdl3_draw_scale(float px)
{
    base_px = px;
    looks[0].px = 0u;
    looks[1].px = 0u;
    forget();
}

uint32_t ed_font_count(void)
{
    scan();

    return found_n;
}

const char *ed_font_name(uint32_t i)
{
    return i < found_n ? founds[i].name : "";
}

int ed_font_use(uint32_t font, const char *name)
{
    uint32_t face;

    if (font > ED_FONT_TEXT)
    {
        return -1;
    }

    if (forced != NONE)
    {
        return 0;
    }

    face = name[0] ? face_named(name) : face_default(font);

    if (face == NONE)
    {
        return -1;
    }

    chosen[font] = face;
    looks[font].px = 0u;
    forget();

    return 0;
}

static int has(uint32_t face, uint32_t cp)
{
    return face != NONE && stbtt_FindGlyphIndex(&faces[face].info, (int)cp) != 0;
}

static uint32_t search(uint32_t cp)
{
    uint32_t before;
    uint32_t face;
    uint32_t i;

    for (i = 0u; i < face_n; i++)
    {
        if (has(i, cp))
        {
            return i;
        }
    }

    for (i = 0u; i < SDL_arraysize(wide_names); i++)
    {
        face = face_named(wide_names[i]);

        if (has(face, cp))
        {
            return face;
        }
    }

    if (searches >= FULL_SEARCHES)
    {
        return NONE;
    }

    searches++;

    for (i = 0u; i < found_n && face_n < FACES; i++)
    {
        before = face_n;
        face = face_load(founds[i].path, founds[i].index);

        if (has(face, cp))
        {
            return face;
        }

        if (face_n > before)
        {
            face_drop();
        }
    }

    return NONE;
}

static void rasterise(struct glyph *g, uint32_t key, uint32_t cp)
{
    uint32_t face = cur->face;
    const stbtt_fontinfo *info;
    float scale = cur->scale;
    int index = stbtt_FindGlyphIndex(&faces[face].info, (int)cp);
    uint8_t *cover;
    uint32_t *pixels;
    SDL_Rect where;
    int advance;
    int bearing;
    int x0;
    int y0;
    int x1;
    int y1;
    int i;

    if (!index && cp > 0x20u)
    {
        face = search(cp);
        face = face == NONE ? cur->face : face;
        index = stbtt_FindGlyphIndex(&faces[face].info, (int)cp);
        scale = face == cur->face ? cur->scale : stbtt_ScaleForMappingEmToPixels(&faces[face].info, cur->scale / stbtt_ScaleForMappingEmToPixels(&faces[cur->face].info, 1.0f));
    }

    info = &faces[face].info;
    stbtt_GetGlyphHMetrics(info, index, &advance, &bearing);
    stbtt_GetGlyphBitmapBox(info, index, scale, scale, &x0, &y0, &x1, &y1);
    g->key = key;
    g->advance = (int32_t)SDL_roundf((float)advance * scale);
    g->w = (int16_t)(x1 - x0);
    g->h = (int16_t)(y1 - y0);
    g->dx = (int16_t)x0;
    g->dy = (int16_t)(cur->baseline + y0);

    if (g->w <= 0 || g->h <= 0 || g->w > ATLAS || g->h > ATLAS)
    {
        g->w = 0;
        return;
    }

    if (shelf_x + g->w > ATLAS)
    {
        shelf_x = 0;
        shelf_y += shelf_h + 1;
        shelf_h = 0;
    }

    cover = SDL_malloc((size_t)g->w * (size_t)g->h);
    pixels = SDL_malloc((size_t)g->w * (size_t)g->h * sizeof *pixels);

    if (shelf_y + g->h > ATLAS || !cover || !pixels)
    {
        SDL_free(cover);
        SDL_free(pixels);
        g->w = 0;
        return;
    }

    stbtt_MakeGlyphBitmap(info, cover, g->w, g->h, g->w, scale, scale, index);

    for (i = 0; i < g->w * g->h; i++)
    {
        ((uint8_t *)&pixels[i])[0] = 255u;
        ((uint8_t *)&pixels[i])[1] = 255u;
        ((uint8_t *)&pixels[i])[2] = 255u;
        ((uint8_t *)&pixels[i])[3] = cover[i];
    }

    where.x = shelf_x;
    where.y = shelf_y;
    where.w = g->w;
    where.h = g->h;
    SDL_UpdateTexture(atlas, &where, pixels, g->w * (int)sizeof *pixels);
    SDL_free(cover);
    SDL_free(pixels);
    g->x = (int16_t)shelf_x;
    g->y = (int16_t)shelf_y;
    shelf_x += g->w + 1;
    shelf_h = g->h > shelf_h ? g->h : shelf_h;
}

static const struct glyph *glyph_of(uint32_t cp)
{
    uint32_t key = (cp + 1u) | cur_font << 21 | cur->px << 22;
    uint32_t i = key * 2654435761u >> 20 & (SLOTS - 1u);

    while (glyphs[i].key && glyphs[i].key != key)
    {
        i = (i + 1u) & (SLOTS - 1u);
    }

    if (glyphs[i].key)
    {
        return &glyphs[i];
    }

    if (glyph_count >= SLOTS / 4u * 3u || shelf_y + cur->line_height * 2 > ATLAS)
    {
        forget();

        return glyph_of(cp);
    }

    glyph_count++;
    rasterise(&glyphs[i], key, cp);

    return &glyphs[i];
}

static uint32_t next(const uint16_t *text, uint32_t n, uint32_t *i)
{
    uint32_t c = text[(*i)++];

    if (c >= 0xD800u && c <= 0xDBFFu && *i < n && text[*i] >= 0xDC00u && text[*i] <= 0xDFFFu)
    {
        c = 0x10000u + ((c - 0xD800u) << 10) + (text[(*i)++] - 0xDC00u);
    }

    return c;
}

int32_t ed_line_height(void)
{
    return cur->line_height;
}

void ed_text_font(uint32_t font, int32_t percent)
{
    uint32_t px = (uint32_t)SDL_roundf(base_px * (float)percent / 100.0f);

    cur_font = font > ED_FONT_TEXT ? ED_FONT_TEXT : font;
    cur = &looks[cur_font];
    px = px < 4u ? 4u : px > 1000u ? 1000u : px;

    if (px == cur->px && cur->face == chosen[cur_font])
    {
        return;
    }

    cur->px = px;
    cur->face = chosen[cur_font];
    measure(cur);
}

int32_t ed_text_width(const uint16_t *text, uint32_t n)
{
    int32_t pen = 0;
    uint32_t i = 0u;

    while (i < n)
    {
        pen += glyph_of(next(text, n, &i))->advance;
    }

    return pen;
}

void ed_draw_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t rgb)
{
    SDL_FRect r = { (float)x, (float)y, (float)w, (float)h };

    SDL_SetRenderDrawColor(sdl3_renderer, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, 255);
    SDL_RenderFillRect(sdl3_renderer, &r);
}

void ed_draw_clip(int32_t x, int32_t y, int32_t w, int32_t h)
{
    SDL_Rect r = { x, y, w, h };

    SDL_SetRenderClipRect(sdl3_renderer, &r);
}

void ed_draw_text(int32_t x, int32_t y, const uint16_t *text, uint32_t n, uint32_t rgb)
{
    const struct glyph *g;
    SDL_FRect from;
    SDL_FRect to;
    int32_t pen = 0;
    uint32_t i = 0u;

    SDL_SetTextureColorMod(atlas, (Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb);

    while (i < n)
    {
        g = glyph_of(next(text, n, &i));

        if (g->w)
        {
            from.x = (float)g->x;
            from.y = (float)g->y;
            from.w = (float)g->w;
            from.h = (float)g->h;
            to.x = (float)(x + pen + g->dx);
            to.y = (float)(y + g->dy);
            to.w = from.w;
            to.h = from.h;
            SDL_RenderTexture(sdl3_renderer, atlas, &from, &to);
        }

        pen += g->advance;
    }
}
