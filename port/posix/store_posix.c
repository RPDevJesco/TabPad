#define _POSIX_C_SOURCE 200809L

#include "store.h"
#include "sess.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DIR_MAX 900u
#define PATH_BYTES 1024u

uint32_t store_puts;
uint32_t store_dels;
uint32_t store_put_bytes;

static char dir[DIR_MAX];
static char put_name[64];
static int put_fd = -1;
static int get_fd = -1;
static DIR *listing;
static uint32_t fail_in;
static uint32_t crash_in;
static uint32_t crash_left;
static int crashing;
static int dead;

static int path_of(char *out, const char *name, const char *suffix)
{
    int n = snprintf(out, PATH_BYTES, "%s/%s%s", dir, name, suffix);

    return n > 0 && (unsigned)n < PATH_BYTES ? 0 : -1;
}

static int to_disk(int fd)
{
#ifdef F_FULLFSYNC
    if (!fcntl(fd, F_FULLFSYNC))
    {
        return 0;
    }
#endif
    return fsync(fd);
}

static int dir_to_disk(void)
{
    int fd = open(dir, O_RDONLY);
    int bad;

    if (fd < 0)
    {
        return -1;
    }

    bad = fsync(fd);
    close(fd);

    return bad;
}

static int is_tmp(const char *name)
{
    size_t n = strlen(name);

    return n > 4u && !strcmp(name + n - 4u, ".tmp");
}

static void close_all(void)
{
    if (put_fd >= 0)
    {
        close(put_fd);
    }

    if (get_fd >= 0)
    {
        close(get_fd);
    }

    if (listing)
    {
        closedir(listing);
    }

    put_fd = -1;
    get_fd = -1;
    listing = 0;
}

int store_dir(const char *path)
{
    char tmp[PATH_BYTES];
    struct dirent *e;
    DIR *d;

    close_all();
    fail_in = 0u;
    crash_in = 0u;
    crashing = 0;
    dead = 0;

    if (strlen(path) >= DIR_MAX || (mkdir(path, 0700) && errno != EEXIST))
    {
        return -1;
    }

    strcpy(dir, path);
    d = opendir(dir);

    if (!d)
    {
        return -1;
    }

    while ((e = readdir(d)) != 0)
    {
        if (is_tmp(e->d_name) && !path_of(tmp, e->d_name, ""))
        {
            unlink(tmp);
        }
    }

    closedir(d);

    return 0;
}

void store_fail(uint32_t nth)
{
    fail_in = nth;
}

void store_crash(uint32_t nth, uint32_t bytes)
{
    crash_in = nth;
    crash_left = bytes;
}

int sess_put_open(const char *name)
{
    char path[PATH_BYTES];

    if (dead || put_fd >= 0 || strlen(name) >= sizeof put_name || (fail_in && !--fail_in))
    {
        return -1;
    }

    crashing = crash_in && !--crash_in;

    if (path_of(path, name, crashing ? "" : ".tmp"))
    {
        return -1;
    }

    strcpy(put_name, name);
    store_put_bytes = 0u;
    put_fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);

    return put_fd < 0 ? -1 : 0;
}

int sess_put_write(const void *buf, uint32_t n)
{
    const uint8_t *p = buf;
    ssize_t k;

    if (dead || put_fd < 0)
    {
        return -1;
    }

    if (crashing && n > crash_left)
    {
        k = write(put_fd, p, crash_left);
        store_put_bytes += crash_left;
        (void)k;
        close_all();
        dead = 1;

        return -1;
    }

    crash_left -= crashing ? n : 0u;
    store_put_bytes += n;

    while (n)
    {
        k = write(put_fd, p, n);

        if (k < 0 && errno == EINTR)
        {
            continue;
        }

        if (k <= 0)
        {
            return -1;
        }

        p += k;
        n -= (uint32_t)k;
    }

    return 0;
}

int sess_put_close(void)
{
    char tmp[PATH_BYTES];
    char path[PATH_BYTES];
    int bad;

    if (dead || put_fd < 0)
    {
        return -1;
    }

    if (crashing)
    {
        close_all();
        dead = 1;

        return -1;
    }

    bad = to_disk(put_fd);
    bad |= close(put_fd);
    put_fd = -1;

    if (path_of(tmp, put_name, ".tmp") || path_of(path, put_name, ""))
    {
        return -1;
    }

    if (bad || rename(tmp, path) || dir_to_disk())
    {
        unlink(tmp);

        return -1;
    }

    store_puts++;

    return 0;
}

int sess_get_open(const char *name, uint32_t *size)
{
    char path[PATH_BYTES];
    struct stat st;

    if (dead || get_fd >= 0 || path_of(path, name, ""))
    {
        return -1;
    }

    get_fd = open(path, O_RDONLY);

    if (get_fd < 0)
    {
        return -1;
    }

    if (fstat(get_fd, &st) || st.st_size < 0 || st.st_size > 0x7FFFFFFF)
    {
        sess_get_close();

        return -1;
    }

    *size = (uint32_t)st.st_size;

    return 0;
}

int sess_get_read(void *buf, uint32_t n)
{
    uint8_t *p = buf;
    ssize_t k;

    if (dead || get_fd < 0)
    {
        return -1;
    }

    while (n)
    {
        k = read(get_fd, p, n);

        if (k < 0 && errno == EINTR)
        {
            continue;
        }

        if (k <= 0)
        {
            return -1;
        }

        p += k;
        n -= (uint32_t)k;
    }

    return 0;
}

void sess_get_close(void)
{
    if (get_fd >= 0)
    {
        close(get_fd);
    }

    get_fd = -1;
}

int sess_del(const char *name)
{
    char path[PATH_BYTES];

    if (dead || path_of(path, name, ""))
    {
        return -1;
    }

    if (!unlink(path))
    {
        store_dels++;

        return 0;
    }

    return errno == ENOENT ? 0 : -1;
}

int sess_list(int first, char *name, uint32_t max)
{
    struct dirent *e;

    if (dead)
    {
        return -1;
    }

    if (first)
    {
        if (listing)
        {
            closedir(listing);
        }

        listing = opendir(dir);
    }

    while (listing && (e = readdir(listing)) != 0)
    {
        if (e->d_name[0] == '.' || is_tmp(e->d_name) || strlen(e->d_name) >= max)
        {
            continue;
        }

        strcpy(name, e->d_name);

        return 0;
    }

    if (listing)
    {
        closedir(listing);
    }

    listing = 0;

    return -1;
}
