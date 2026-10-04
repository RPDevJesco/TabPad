#include "sdl3.h"
#include <SDL3/SDL_main.h>
#include "desk.h"
#include "ed.h"
#include "store.h"

static const char *const plain_exts[] = { ".txt", ".log", ".ini", ".cfg", ".conf", ".yml", ".yaml", ".toml" };

#define FIRST_W 1000
#define FIRST_H 700
#define FONT_PX 15.0f
#define TICK_MS 200
#define PLAIN_EXTS (sizeof plain_exts / sizeof plain_exts[0])

SDL_Renderer *sdl3_renderer;

#ifdef SDL_PLATFORM_WINDOWS
const uint32_t ed_eol_default = ED_EOL_CRLF;
#else
const uint32_t ed_eol_default = ED_EOL_LF;
#endif

static SDL_Window *window;
static int32_t scale_forced;
static int32_t scale_now;
static Uint32 picked_event;
static char *clip;


int ed_file_stat(const char *path, uint64_t *size, uint64_t *mtime)
{
    SDL_PathInfo info;

    if (!SDL_GetPathInfo(path, &info) || info.type != SDL_PATHTYPE_FILE)
    {
        return -1;
    }

    *size = info.size;
    *mtime = (uint64_t)info.modify_time;

    return 0;
}

int ed_file_get(const char *path, void *buf, uint32_t n)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    size_t got;

    if (!io)
    {
        return -1;
    }

    got = SDL_ReadIO(io, buf, n);
    SDL_CloseIO(io);

    return got == n ? 0 : -1;
}

int ed_file_put(const char *path, const void *buf, uint32_t n)
{
    SDL_IOStream *io = SDL_IOFromFile(path, "wb");
    int bad;

    if (!io)
    {
        return -1;
    }

    bad = SDL_WriteIO(io, buf, n) != n;
    bad |= !SDL_FlushIO(io);
    bad |= !SDL_CloseIO(io);

    return bad ? -1 : 0;
}

int ed_clip_put(const char *utf8, uint32_t n)
{
    char *text = SDL_malloc((size_t)n + 1u);
    int bad;

    if (!text)
    {
        return -1;
    }

    SDL_memcpy(text, utf8, n);
    text[n] = 0;
    bad = !SDL_SetClipboardText(text);
    SDL_free(text);

    return bad ? -1 : 0;
}

const char *ed_clip_get(uint32_t *n)
{
    SDL_free(clip);
    clip = SDL_HasClipboardText() ? SDL_GetClipboardText() : 0;
    *n = clip ? (uint32_t)SDL_strlen(clip) : 0u;

    return clip;
}

static void SDLCALL picked(void *what, const char *const *files, int filter)
{
    SDL_Event e;

    (void)filter;
    SDL_zero(e);
    e.type = picked_event;
    e.user.code = (Sint32)(intptr_t)what;
    e.user.data1 = files && files[0] ? SDL_strdup(files[0]) : 0;
    SDL_PushEvent(&e);
}

void ed_pick(uint32_t what)
{
    if (what == ED_PICK_SAVE)
    {
        SDL_ShowSaveFileDialog(picked, (void *)(intptr_t)what, window, 0, 0, 0);
        return;
    }

    SDL_ShowOpenFileDialog(picked, (void *)(intptr_t)what, window, 0, 0, 0, false);
}

uint32_t ed_ask_close(const char *name)
{
    SDL_MessageBoxButtonData buttons[3];
    SDL_MessageBoxData box;
    char text[512];
    int chosen = (int)ED_CLOSE_CANCEL;

    SDL_zero(buttons);
    buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    buttons[0].buttonID = (int)ED_CLOSE_SAVE;
    buttons[0].text = ed_word(ED_W_ASK_SAVE);
    buttons[1].buttonID = (int)ED_CLOSE_DISCARD;
    buttons[1].text = ed_word(ED_W_ASK_DISCARD);
    buttons[2].flags = SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    buttons[2].buttonID = (int)ED_CLOSE_CANCEL;
    buttons[2].text = ed_word(ED_W_ASK_CANCEL);
    SDL_snprintf(text, sizeof text, "\"%s\" %s", name, ed_word(ED_W_ASK_TEXT));
    SDL_zero(box);
    box.flags = SDL_MESSAGEBOX_WARNING;
    box.window = window;
    box.title = ed_word(ED_W_ASK_TITLE);
    box.message = text;
    box.numbuttons = 3;
    box.buttons = buttons;

    if (!SDL_ShowMessageBox(&box, &chosen))
    {
        return ED_CLOSE_CANCEL;
    }

    return (uint32_t)chosen;
}

const int ed_erase_offered = 0;

void ed_erase(void)
{
}

void ed_exit(void)
{
    SDL_Event e;

    SDL_zero(e);
    e.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&e);
}

void ed_title(const char *name, int unsaved)
{
    char text[512];

    SDL_snprintf(text, sizeof text, "%s%s - TabPad", name, unsaved ? " *" : "");
    SDL_SetWindowTitle(window, text);
}

static int32_t scale_of(void)
{
    float scale = SDL_GetWindowDisplayScale(window);

    if (scale_forced)
    {
        return scale_forced;
    }

    return scale > 0.0f ? (int32_t)SDL_roundf(scale * 100.0f) : 100;
}

static void sized(void)
{
    int w;
    int h;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    if (scale_of() != scale_now)
    {
        scale_now = scale_of();
        sdl3_draw_scale(FONT_PX * (float)scale_now / 100.0f);
    }

    ed_resize(w, h, scale_now);
}

static void first_size(void)
{
    float density = SDL_GetWindowPixelDensity(window);
    float scale = SDL_GetWindowDisplayScale(window);

    if (!scale_forced && density > 0.0f && scale > density)
    {
        SDL_SetWindowSize(window, (int)((float)FIRST_W * scale / density), (int)((float)FIRST_H * scale / density));
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

static int32_t px(float v)
{
    return (int32_t)(v * SDL_GetWindowPixelDensity(window));
}

static uint32_t mods_of(SDL_Keymod m)
{
    uint32_t mods = 0u;

    if (m & SDL_KMOD_SHIFT)
    {
        mods |= ED_MOD_SHIFT;
    }

    if (m & (SDL_KMOD_CTRL | SDL_KMOD_GUI))
    {
        mods |= ED_MOD_CTRL;
    }

    if (m & SDL_KMOD_ALT)
    {
        mods |= ED_MOD_ALT;
    }

    return mods;
}

static void on_key(const SDL_KeyboardEvent *k)
{
    uint32_t mods = mods_of(k->mod);

    switch (k->key)
    {
        case SDLK_LEFT: ed_key(ED_KEY_LEFT, mods); break;
        case SDLK_RIGHT: ed_key(ED_KEY_RIGHT, mods); break;
        case SDLK_UP: ed_key(ED_KEY_UP, mods); break;
        case SDLK_DOWN: ed_key(ED_KEY_DOWN, mods); break;
        case SDLK_HOME: ed_key(ED_KEY_HOME, mods); break;
        case SDLK_END: ed_key(ED_KEY_END, mods); break;
        case SDLK_PAGEUP: ed_key(ED_KEY_PAGE_UP, mods); break;
        case SDLK_PAGEDOWN: ed_key(ED_KEY_PAGE_DOWN, mods); break;
        case SDLK_BACKSPACE: ed_key(ED_KEY_BACKSPACE, mods); break;
        case SDLK_DELETE: ed_key(ED_KEY_DELETE, mods); break;
        case SDLK_RETURN: ed_key(ED_KEY_ENTER, mods); break;
        case SDLK_KP_ENTER: ed_key(ED_KEY_ENTER, mods); break;
        case SDLK_TAB: ed_key(ED_KEY_TAB, mods); break;
        case SDLK_ESCAPE: ed_key(ED_KEY_ESCAPE, mods); break;
        case SDLK_INSERT: ed_key(ED_KEY_INSERT, mods); break;
        case SDLK_F3: ed_key(ED_KEY_F3, mods); break;
        case SDLK_KP_PLUS: ed_key('=', mods); break;
        case SDLK_KP_MINUS: ed_key('-', mods); break;

        default:
            if ((mods & (ED_MOD_CTRL | ED_MOD_ALT)) && k->key >= 0x20u && k->key < 0x7Fu)
            {
                ed_key((uint32_t)k->key, mods);
            }

            break;
    }
}

static int on_event(const SDL_Event *e)
{
    if (e->type == picked_event)
    {
        ed_picked((uint32_t)e->user.code, e->user.data1);
        SDL_free(e->user.data1);

        return 1;
    }

    switch (e->type)
    {
        case SDL_EVENT_QUIT:
            ed_focus_lost();

            return 0;

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_EXPOSED:
            sized();
            break;

        case SDL_EVENT_WINDOW_FOCUS_LOST:
            ed_focus_lost();
            break;

        case SDL_EVENT_KEY_DOWN:
            on_key(&e->key);
            break;

        case SDL_EVENT_TEXT_INPUT:
            ed_text(e->text.text, (uint32_t)SDL_strlen(e->text.text));
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (e->button.button == SDL_BUTTON_LEFT)
            {
                ed_mouse_down(px(e->button.x), px(e->button.y), mods_of(SDL_GetModState()), e->button.clicks);
            }

            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (e->button.button == SDL_BUTTON_LEFT)
            {
                ed_mouse_up();
            }

            break;

        case SDL_EVENT_MOUSE_MOTION:
            ed_mouse_move(px(e->motion.x), px(e->motion.y));
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            ed_wheel((int32_t)-e->wheel.y, mods_of(SDL_GetModState()));
            break;

        case SDL_EVENT_DROP_FILE:
            ed_open(e->drop.data);
            break;

        default:
            break;
    }

    return 1;
}

static void paint(void)
{
    SDL_SetRenderClipRect(sdl3_renderer, 0);
    SDL_SetRenderDrawColor(sdl3_renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl3_renderer);
    ed_draw();
}

static int shoot(const char *path)
{
    SDL_Surface *shot;
    int bad;

    paint();
    shot = SDL_RenderReadPixels(sdl3_renderer, 0);
    bad = !shot || !SDL_SaveBMP(shot, path);
    SDL_DestroySurface(shot);
    SDL_RenderPresent(sdl3_renderer);

    return bad;
}

static int session_path(const char *dir, char *out, size_t max)
{
    char *pref;
    size_t n;

    if (dir)
    {
        if (SDL_snprintf(out, max, "%s", dir) >= (int)max)
        {
            return -1;
        }

        for (n = SDL_strlen(out); n > 1u && (out[n - 1u] == '/' || out[n - 1u] == '\\'); n--)
        {
            out[n - 1u] = 0;
        }

        return 0;
    }

    pref = SDL_GetPrefPath(0, "tabpad");

    if (!pref)
    {
        return -1;
    }

    SDL_snprintf(out, max, "%ssession", pref);
    SDL_free(pref);

    return 0;
}

static void whole(const char *arg, char *out, size_t max)
{
    char *here = SDL_GetCurrentDirectory();
    int is_whole = arg[0] == '/' || arg[0] == '\\' || (arg[0] && arg[1] == ':');

    SDL_snprintf(out, max, "%s%s", is_whole || !here ? "" : here, arg);
    SDL_free(here);
}

static void inbox_send(const char *inbox, int argc, char **argv, int first)
{
    char path[2048];
    SDL_IOStream *io = SDL_IOFromFile(inbox, "ab");
    int i;

    if (!io)
    {
        return;
    }

    for (i = first; i < argc; i++)
    {
        whole(argv[i], path, sizeof path);
        SDL_WriteIO(io, path, SDL_strlen(path));
        SDL_WriteIO(io, "\n", 1u);
    }

    if (first >= argc)
    {
        SDL_WriteIO(io, "\n", 1u);
    }

    SDL_CloseIO(io);
}

static void inbox_take(const char *inbox)
{
    char taken[1100];
    char *text;
    char *line;
    char *end;
    size_t n = 0u;

    SDL_snprintf(taken, sizeof taken, "%s.taken", inbox);

    if (!SDL_RenamePath(inbox, taken))
    {
        return;
    }

    text = SDL_LoadFile(taken, &n);
    SDL_RemovePath(taken);

    for (line = text; line && line < text + n; line = end + 1)
    {
        end = SDL_strchr(line, '\n');

        if (!end)
        {
            break;
        }

        *end = 0;

        if (line[0])
        {
            ed_open(line);
        }
    }

    SDL_free(text);
    SDL_RestoreWindow(window);
    SDL_RaiseWindow(window);
}

static const char languages_readme[] =
    "Languages added to TabPad: one folder each, named after the language.\n"
    "\n"
    "  zig/\n"
    "      language.ini      title = Zig\n"
    "                        name = Zig source file\n"
    "                        endings = .zig .zon\n"
    "      zig.dll           the Tree-sitter grammar as a shared library (zig.so on Linux,\n"
    "                        zig.dylib on macOS), exporting tree_sitter_zig\n"
    "      highlights.scm    the grammar's highlight query\n"
    "\n"
    "A folder named after a language TabPad already has takes it over; it may hold only\n"
    "a highlights.scm, to change that language's colours.\n"
    "TabPad reads this folder when it starts. languages.log says what became of each folder.\n"
    "A library is a program: add only ones you trust.\n";

static void languages(const char *dir)
{
    char path[1100];
    char file[1200];
    const char *base = SDL_GetBasePath();
    char *pref = dir ? 0 : SDL_GetPrefPath(0, "tabpad");
    SDL_IOStream *log;

    if (dir)
    {
        SDL_snprintf(path, sizeof path, "%s", dir);
    }

    else
    {
        SDL_snprintf(path, sizeof path, "%slanguages", pref ? pref : "");
    }

    SDL_free(pref);
    SDL_snprintf(file, sizeof file, "%s/README.txt", path);

    if (!SDL_GetPathInfo(path, 0) && SDL_CreateDirectory(path))
    {
        SDL_SaveFile(file, languages_readme, sizeof languages_readme - 1u);
    }

    SDL_snprintf(file, sizeof file, "%s/languages.log", path);
    log = SDL_IOFromFile(file, "wb");

    if (base)
    {
        SDL_snprintf(file, sizeof file, "%slanguages", base);
        sdl3_languages(file, log);
    }

    sdl3_languages(path, log);

    if (log)
    {
        SDL_CloseIO(log);
    }
}

static int registering(const char *argv0, int on)
{
    const char **exts = SDL_malloc((ed_ext_count + PLAIN_EXTS) * sizeof *exts);
    const char *video = SDL_getenv("SDL_VIDEODRIVER");
    uint32_t i;
    int bad;

    if (!exts)
    {
        return 1;
    }

    for (i = 0u; i < ed_ext_count; i++)
    {
        exts[i] = ed_exts[i].ext;
    }

    for (i = 0u; i < PLAIN_EXTS; i++)
    {
        exts[ed_ext_count + i] = plain_exts[i];
    }

    bad = on ? desk_register(argv0, exts, ed_ext_count + (uint32_t)PLAIN_EXTS) : desk_unregister(exts, ed_ext_count + (uint32_t)PLAIN_EXTS);
    SDL_free(exts);
    SDL_Log("tabpad: %s%s", on ? "registered" : "unregistered", bad ? ": FAILED" : "");

    if (!video || SDL_strcmp(video, "dummy"))
    {
        SDL_ShowSimpleMessageBox(bad ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION, "TabPad", bad ? "The system's settings could not be changed." : on ? "Registered. TabPad is now offered under \"Open with\" and in the default apps." : "Unregistered.", 0);
    }

    return bad ? 1 : 0;
}

static void post_click(const char *at)
{
    SDL_Event e;
    char *rest;
    float x = (float)SDL_strtol(at, &rest, 10);
    float y = (float)SDL_strtol(rest[0] ? rest + 1 : rest, 0, 10);

    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.x = x;
    e.motion.y = y;
    SDL_PushEvent(&e);
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.clicks = 1;
    e.button.x = x;
    e.button.y = y;
    SDL_PushEvent(&e);
    e.type = SDL_EVENT_MOUSE_BUTTON_UP;
    SDL_PushEvent(&e);
}

int main(int argc, char **argv)
{
    const char *session = 0;
    const char *font = 0;
    const char *shot = 0;
    const char *typed = 0;
    const char *clicks[16];
    int click_n = 0;
    const char *added = 0;
    const char *words = 0;
    char dir[1024];
    char lock[1100];
    char inbox[1100];
    char path[2048];
    SDL_Event e;
    int running = 1;
    int w;
    int h;
    int i;

    if (argc == 2 && (!SDL_strcmp(argv[1], "--register") || !SDL_strcmp(argv[1], "--unregister")))
    {
        languages(0);

        return registering(argv[0], !SDL_strcmp(argv[1], "--register"));
    }

    for (i = 1; i + 1 < argc && argv[i][0] == '-' && argv[i][1] == '-'; i += 2)
    {
        if (!SDL_strcmp(argv[i], "--session"))
        {
            session = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--font"))
        {
            font = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--shot"))
        {
            shot = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--type"))
        {
            typed = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--click") && click_n < 16)
        {
            clicks[click_n++] = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--languages"))
        {
            added = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--translations"))
        {
            words = argv[i + 1];
        }

        else if (!SDL_strcmp(argv[i], "--scale"))
        {
            scale_forced = SDL_atoi(argv[i + 1]);
        }
    }

    if (session_path(session, dir, sizeof dir))
    {
        SDL_Log("tabpad: there is no place for the session");

        return 1;
    }

    SDL_snprintf(lock, sizeof lock, "%s.lock", dir);
    SDL_snprintf(inbox, sizeof inbox, "%s.inbox", dir);

    if (!desk_claim(lock))
    {
        inbox_send(inbox, argc, argv, i);
        desk_hand_over();

        return 0;
    }

    languages(added);

    if (!SDL_Init(SDL_INIT_VIDEO) || !SDL_CreateWindowAndRenderer("TabPad", FIRST_W, FIRST_H, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &sdl3_renderer))
    {
        SDL_Log("tabpad: %s", SDL_GetError());

        return 1;
    }

    picked_event = SDL_RegisterEvents(1);
    first_size();
    scale_now = scale_of();
    sdl3_tongues(words);
    sdl3_draw_open(font, FONT_PX * (float)scale_now / 100.0f);
    SDL_GetWindowSizeInPixels(window, &w, &h);

    if (store_dir(dir) || ed_init(w, h, scale_now))
    {
        SDL_Log("tabpad: the session could not be opened");

        return 1;
    }

    for (; i < argc; i++)
    {
        whole(argv[i], path, sizeof path);
        ed_open(path);
    }

    SDL_StartTextInput(window);

    for (w = 0; w < click_n; w++)
    {
        post_click(clicks[w]);
    }

    if (typed)
    {
        SDL_zero(e);
        e.type = SDL_EVENT_TEXT_INPUT;
        e.text.text = typed;
        SDL_PushEvent(&e);
    }

    if (shot)
    {
        SDL_zero(e);
        e.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&e);

        while (SDL_PollEvent(&e) && e.type != SDL_EVENT_QUIT)
        {
            on_event(&e);
        }

        i = shoot(shot);
        on_event(&e);

        return i;
    }

    while (running)
    {
        if (SDL_WaitEventTimeout(&e, TICK_MS))
        {
            do
            {
                running = running && on_event(&e);
            } while (SDL_PollEvent(&e));
        }

        ed_tick(SDL_GetTicks());
        inbox_take(inbox);

        if (ed_stale())
        {
            paint();
            SDL_RenderPresent(sdl3_renderer);
        }
    }

    return 0;
}
