#define _XOPEN_SOURCE 700

#include "desk.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_BYTES 4096u

static const char types[] =
    "text/plain;text/x-csrc;text/x-chdr;text/x-c++src;text/x-c++hdr;text/x-csharp;text/css;"
    "text/javascript;application/javascript;text/html;text/x-lua;text/x-apex;text/x-python;"
    "application/x-ruby;text/x-ruby;text/rust;text/markdown;application/json;application/xml;text/xml;"
    "application/x-shellscript;application/x-powershell;";

static int entries(char *out)
{
    const char *data = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int n;

    if (data && data[0])
    {
        n = snprintf(out, PATH_BYTES, "%s", data);
    }

    else if (home && home[0])
    {
        n = snprintf(out, PATH_BYTES, "%s/.local/share", home);
    }

    else
    {
        return -1;
    }

    if (n < 0 || (size_t)n > PATH_BYTES - 64u)
    {
        return -1;
    }

    mkdir(out, 0700);
    strcat(out, "/applications");
    mkdir(out, 0700);

    return 0;
}

static int self(const char *argv0, char *out)
{
    ssize_t n;

    if (argv0 && strchr(argv0, '/') && realpath(argv0, out))
    {
        return 0;
    }

    n = readlink("/proc/self/exe", out, PATH_MAX - 1u);

    if (n <= 0)
    {
        return -1;
    }

    out[n] = 0;

    return 0;
}

int desk_register(const char *argv0, const char *const *exts, uint32_t n)
{
    char exe[PATH_MAX];
    char path[PATH_BYTES];
    FILE *f;
    size_t i;

    (void)exts;
    (void)n;

    if (self(argv0, exe) || entries(path))
    {
        return -1;
    }

    strcat(path, "/tabpad.desktop");
    f = fopen(path, "w");

    if (!f)
    {
        return -1;
    }

    fputs("[Desktop Entry]\nType=Application\nName=TabPad\nGenericName=Text Editor\n", f);
    fputs("Comment=Tabs of text that are still there the next time\nIcon=accessories-text-editor\n", f);
    fputs("Terminal=false\nCategories=Utility;TextEditor;Development;\nExec=\"", f);

    for (i = 0u; exe[i]; i++)
    {
        if (exe[i] == '"' || exe[i] == '`' || exe[i] == '$' || exe[i] == '\\')
        {
            fputc('\\', f);
        }

        fputc(exe[i], f);
    }

    fprintf(f, "\" %%F\nMimeType=%s\n", types);

    return fclose(f) ? -1 : 0;
}

int desk_unregister(const char *const *exts, uint32_t n)
{
    char path[PATH_BYTES];

    (void)exts;
    (void)n;

    if (entries(path))
    {
        return -1;
    }

    strcat(path, "/tabpad.desktop");

    return remove(path) && errno != ENOENT ? -1 : 0;
}

int desk_claim(const char *path)
{
    struct flock lock;
    int fd = open(path, O_RDWR | O_CREAT, 0600);

    if (fd < 0)
    {
        return -1;
    }

    memset(&lock, 0, sizeof lock);
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;

    if (!fcntl(fd, F_SETLK, &lock))
    {
        return 1;
    }

    close(fd);

    return errno == EACCES || errno == EAGAIN ? 0 : -1;
}

void desk_hand_over(void)
{
}

static void line(FILE *f, char *out, uint32_t max)
{
    size_t n;

    out[0] = 0;

    if (!fgets(out, (int)max, f))
    {
        return;
    }

    n = strlen(out);

    if (n && out[n - 1u] == '\n')
    {
        out[n - 1u] = 0;
    }
}

int desk_font(int which, char *name, uint32_t name_max, char *path, uint32_t path_max, int *index)
{
    FILE *f = popen(which == DESK_FONT_UI ? "fc-match -f '%{family[0]}\\n%{file}\\n%{index}\\n' sans-serif 2>/dev/null" : "fc-match -f '%{family[0]}\\n%{file}\\n%{index}\\n' monospace 2>/dev/null", "r");
    char number[32];

    name[0] = 0;
    path[0] = 0;
    *index = 0;

    if (!f)
    {
        return -1;
    }

    line(f, name, name_max);
    line(f, path, path_max);
    line(f, number, sizeof number);
    pclose(f);
    *index = atoi(number);

    return name[0] ? 0 : -1;
}
