#include "store.h"
#include "sess.h"
#include <windows.h>

#define DIR_MAX 900u
#define PATH_UNITS 1024u

uint32_t store_puts;
uint32_t store_dels;
uint32_t store_put_bytes;

static WCHAR dir[DIR_MAX];
static char put_name[64];
static HANDLE put_h = INVALID_HANDLE_VALUE;
static HANDLE get_h = INVALID_HANDLE_VALUE;
static HANDLE listing = INVALID_HANDLE_VALUE;
static uint32_t fail_in;
static uint32_t crash_in;
static uint32_t crash_left;
static int crashing;
static int dead;

static int path_of(WCHAR *out, const char *name, const char *suffix)
{
    uint32_t n = 0u;
    uint32_t i;

    for (i = 0u; dir[i]; i++)
    {
        out[n++] = dir[i];
    }

    out[n++] = L'\\';

    for (i = 0u; name[i] && n < PATH_UNITS - 1u; i++)
    {
        out[n++] = (WCHAR)(unsigned char)name[i];
    }

    for (i = 0u; suffix[i] && n < PATH_UNITS - 1u; i++)
    {
        out[n++] = (WCHAR)(unsigned char)suffix[i];
    }

    out[n] = 0;

    return n < PATH_UNITS - 1u ? 0 : -1;
}

static int is_tmp(const WCHAR *name)
{
    size_t n = 0u;

    while (name[n])
    {
        n++;
    }

    return n > 4u && name[n - 4u] == L'.' && name[n - 3u] == L't' && name[n - 2u] == L'm' && name[n - 1u] == L'p';
}

static void close_all(void)
{
    if (put_h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(put_h);
    }

    if (get_h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(get_h);
    }

    if (listing != INVALID_HANDLE_VALUE)
    {
        FindClose(listing);
    }

    put_h = INVALID_HANDLE_VALUE;
    get_h = INVALID_HANDLE_VALUE;
    listing = INVALID_HANDLE_VALUE;
}

static int write_all(HANDLE h, const uint8_t *p, uint32_t n)
{
    DWORD k;

    while (n)
    {
        if (!WriteFile(h, p, n, &k, 0) || !k)
        {
            return -1;
        }

        p += k;
        n -= k;
    }

    return 0;
}

int store_dir(const char *path)
{
    WCHAR pattern[PATH_UNITS];
    WCHAR tmp[PATH_UNITS];
    WIN32_FIND_DATAW e;
    HANDLE find;
    uint32_t n = 0u;
    uint32_t i;

    close_all();
    fail_in = 0u;
    crash_in = 0u;
    crashing = 0;
    dead = 0;

    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, dir, DIR_MAX))
    {
        return -1;
    }

    if (!CreateDirectoryW(dir, 0) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return -1;
    }

    if (path_of(pattern, "*.tmp", ""))
    {
        return -1;
    }

    find = FindFirstFileW(pattern, &e);

    while (find != INVALID_HANDLE_VALUE)
    {
        for (i = 0u; dir[i]; i++)
        {
            tmp[i] = dir[i];
        }

        tmp[i++] = L'\\';

        for (n = 0u; e.cFileName[n] && i < PATH_UNITS - 1u; n++)
        {
            tmp[i++] = e.cFileName[n];
        }

        tmp[i] = 0;
        DeleteFileW(tmp);

        if (!FindNextFileW(find, &e))
        {
            FindClose(find);
            find = INVALID_HANDLE_VALUE;
        }
    }

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
    WCHAR path[PATH_UNITS];
    uint32_t i;

    if (dead || put_h != INVALID_HANDLE_VALUE || lstrlenA(name) >= (int)sizeof put_name || (fail_in && !--fail_in))
    {
        return -1;
    }

    crashing = crash_in && !--crash_in;

    if (path_of(path, name, crashing ? "" : ".tmp"))
    {
        return -1;
    }

    for (i = 0u; name[i]; i++)
    {
        put_name[i] = name[i];
    }

    put_name[i] = 0;
    store_put_bytes = 0u;
    put_h = CreateFileW(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    return put_h == INVALID_HANDLE_VALUE ? -1 : 0;
}

int sess_put_write(const void *buf, uint32_t n)
{
    if (dead || put_h == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (crashing && n > crash_left)
    {
        write_all(put_h, buf, crash_left);
        store_put_bytes += crash_left;
        close_all();
        dead = 1;

        return -1;
    }

    crash_left -= crashing ? n : 0u;
    store_put_bytes += n;

    return write_all(put_h, buf, n);
}

int sess_put_close(void)
{
    WCHAR tmp[PATH_UNITS];
    WCHAR path[PATH_UNITS];
    int bad;

    if (dead || put_h == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (crashing)
    {
        close_all();
        dead = 1;

        return -1;
    }

    bad = !FlushFileBuffers(put_h);
    bad |= !CloseHandle(put_h);
    put_h = INVALID_HANDLE_VALUE;

    if (path_of(tmp, put_name, ".tmp") || path_of(path, put_name, ""))
    {
        return -1;
    }

    if (bad || !MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        DeleteFileW(tmp);

        return -1;
    }

    store_puts++;

    return 0;
}

int sess_get_open(const char *name, uint32_t *size)
{
    WCHAR path[PATH_UNITS];
    LARGE_INTEGER big;

    if (dead || get_h != INVALID_HANDLE_VALUE || path_of(path, name, ""))
    {
        return -1;
    }

    get_h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (get_h == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    if (!GetFileSizeEx(get_h, &big) || big.QuadPart > 0x7FFFFFFF)
    {
        sess_get_close();

        return -1;
    }

    *size = (uint32_t)big.QuadPart;

    return 0;
}

int sess_get_read(void *buf, uint32_t n)
{
    uint8_t *p = buf;
    DWORD k;

    if (dead || get_h == INVALID_HANDLE_VALUE)
    {
        return -1;
    }

    while (n)
    {
        if (!ReadFile(get_h, p, n, &k, 0) || !k)
        {
            return -1;
        }

        p += k;
        n -= k;
    }

    return 0;
}

void sess_get_close(void)
{
    if (get_h != INVALID_HANDLE_VALUE)
    {
        CloseHandle(get_h);
    }

    get_h = INVALID_HANDLE_VALUE;
}

int sess_del(const char *name)
{
    WCHAR path[PATH_UNITS];

    if (dead || path_of(path, name, ""))
    {
        return -1;
    }

    if (DeleteFileW(path))
    {
        store_dels++;

        return 0;
    }

    return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : -1;
}

int sess_list(int first, char *name, uint32_t max)
{
    WCHAR pattern[PATH_UNITS];
    WIN32_FIND_DATAW e;
    uint32_t i;
    int have = 0;

    if (dead)
    {
        return -1;
    }

    if (first)
    {
        if (listing != INVALID_HANDLE_VALUE)
        {
            FindClose(listing);
        }

        listing = path_of(pattern, "*", "") ? INVALID_HANDLE_VALUE : FindFirstFileW(pattern, &e);
        have = listing != INVALID_HANDLE_VALUE;
    }

    while (listing != INVALID_HANDLE_VALUE)
    {
        if (!have && !FindNextFileW(listing, &e))
        {
            break;
        }

        have = 0;

        if ((e.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || is_tmp(e.cFileName))
        {
            continue;
        }

        for (i = 0u; e.cFileName[i] && e.cFileName[i] < 0x80 && i < max - 1u; i++)
        {
            name[i] = (char)e.cFileName[i];
        }

        if (e.cFileName[i])
        {
            continue;
        }

        name[i] = 0;

        return 0;
    }

    if (listing != INVALID_HANDLE_VALUE)
    {
        FindClose(listing);
    }

    listing = INVALID_HANDLE_VALUE;

    return -1;
}
