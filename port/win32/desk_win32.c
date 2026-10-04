#include "desk.h"
#include <windows.h>
#include <shlobj.h>

#define PATH_UNITS 1024u
#define TEXT_UNITS 1400u
#define CLASSES L"Software\\Classes\\"
#define PROG L"tabpad.file"
#define CAPS L"Software\\tabpad\\Capabilities"
#define APP_NAME L"TabPad"

static HANDLE lock = INVALID_HANDLE_VALUE;

static int put(const WCHAR *key, const WCHAR *name, const WCHAR *value)
{
    return RegSetKeyValueW(HKEY_CURRENT_USER, key, name, REG_SZ, value, (DWORD)((lstrlenW(value) + 1) * (int)sizeof *value)) != ERROR_SUCCESS;
}

static const WCHAR *join(WCHAR *out, const WCHAR *a, const WCHAR *b, const WCHAR *c)
{
    lstrcpynW(out, a, (int)TEXT_UNITS);
    lstrcpynW(out + lstrlenW(out), b, (int)TEXT_UNITS - lstrlenW(out));
    lstrcpynW(out + lstrlenW(out), c, (int)TEXT_UNITS - lstrlenW(out));

    return out;
}

static const WCHAR *self(WCHAR *exe)
{
    const WCHAR *name = exe;
    DWORD n = GetModuleFileNameW(0, exe, PATH_UNITS);
    DWORD i;

    exe[n < PATH_UNITS ? n : 0u] = 0;

    for (i = 0u; exe[i]; i++)
    {
        if (exe[i] == L'\\')
        {
            name = exe + i + 1u;
        }
    }

    return name;
}

static void tell_the_shell(void)
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
}

int desk_register(const char *argv0, const char *const *exts, uint32_t n)
{
    WCHAR exe[PATH_UNITS];
    WCHAR key[TEXT_UNITS];
    WCHAR command[TEXT_UNITS];
    WCHAR icon[TEXT_UNITS];
    WCHAR ext[64];
    const WCHAR *name = self(exe);
    uint32_t i;
    int bad = !exe[0];

    (void)argv0;
    join(command, L"\"", exe, L"\" \"%1\"");
    join(icon, L"\"", exe, L"\",0");

    bad |= put(CLASSES PROG L"\\DefaultIcon", 0, icon);
    bad |= put(CLASSES PROG L"\\shell\\open\\command", 0, command);

    join(key, CLASSES L"Applications\\", name, L"");

    bad |= put(key, L"FriendlyAppName", APP_NAME);

    join(key, CLASSES L"Applications\\", name, L"\\shell\\open\\command");

    bad |= put(key, 0, command);
    bad |= put(CAPS, L"ApplicationName", APP_NAME);
    bad |= put(CAPS, L"ApplicationDescription", L"Tabs of text that are still there the next time");
    bad |= put(L"Software\\RegisteredApplications", L"tabpad", CAPS);
    join(key, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", name, L"");

    bad |= put(key, 0, exe);
    bad |= put(CLASSES L"*\\shell\\tabpad", 0, L"Edit with TabPad");
    bad |= put(CLASSES L"*\\shell\\tabpad", L"Icon", icon);
    bad |= put(CLASSES L"*\\shell\\tabpad\\command", 0, command);

    for (i = 0u; i < n; i++)
    {
        if (!MultiByteToWideChar(CP_UTF8, 0, exts[i], -1, ext, 64))
        {
            bad = 1;
            continue;
        }

        join(key, CLASSES L"Applications\\", name, L"\\SupportedTypes");
        bad |= put(key, ext, L"");
        join(key, CLASSES, ext, L"\\OpenWithProgids");
        bad |= put(key, PROG, L"");
        bad |= put(CAPS L"\\FileAssociations", ext, PROG);
    }

    tell_the_shell();

    return bad ? -1 : 0;
}

int desk_unregister(const char *const *exts, uint32_t n)
{
    WCHAR exe[PATH_UNITS];
    WCHAR key[TEXT_UNITS];
    WCHAR ext[64];
    const WCHAR *name = self(exe);
    uint32_t i;

    RegDeleteTreeW(HKEY_CURRENT_USER, CLASSES PROG);
    RegDeleteTreeW(HKEY_CURRENT_USER, join(key, CLASSES L"Applications\\", name, L""));
    RegDeleteTreeW(HKEY_CURRENT_USER, L"Software\\tabpad");
    RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", L"tabpad");
    RegDeleteTreeW(HKEY_CURRENT_USER, join(key, L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\", name, L""));
    RegDeleteTreeW(HKEY_CURRENT_USER, CLASSES L"*\\shell\\tabpad");

    for (i = 0u; i < n; i++)
    {
        if (MultiByteToWideChar(CP_UTF8, 0, exts[i], -1, ext, 64))
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER, join(key, CLASSES, ext, L"\\OpenWithProgids"), PROG);
        }
    }

    tell_the_shell();

    return 0;
}

int desk_claim(const char *path)
{
    WCHAR wide[PATH_UNITS];

    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, (int)PATH_UNITS))
    {
        return -1;
    }

    lock = CreateFileW(wide, GENERIC_WRITE, 0u, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if (lock != INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    return GetLastError() == ERROR_SHARING_VIOLATION ? 0 : -1;
}

void desk_hand_over(void)
{
    AllowSetForegroundWindow(ASFW_ANY);
}


int desk_font(int which, char *name, uint32_t name_max, char *path, uint32_t path_max, int *index)
{
    NONCLIENTMETRICSW m;
    WCHAR face[LF_FACESIZE + 1];
    WCHAR value[TEXT_UNITS];
    WCHAR file[PATH_UNITS];
    WCHAR whole[TEXT_UNITS];
    WCHAR dir[PATH_UNITS];
    DWORD size = sizeof file;
    DWORD i;
    int rooted = 0;

    name[0] = 0;
    path[0] = 0;
    *index = 0;
    m.cbSize = sizeof m;
    lstrcpynW(face, L"Consolas", LF_FACESIZE);

    if (which == DESK_FONT_UI)
    {
        if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof m, &m, 0))
        {
            return -1;
        }

        lstrcpynW(face, m.lfMessageFont.lfFaceName, LF_FACESIZE);
    }

    if (!WideCharToMultiByte(CP_UTF8, 0, face, -1, name, (int)name_max, 0, 0))
    {
        name[0] = 0;

        return -1;
    }

    join(value, face, L" (TrueType)", L"");

    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", value, RRF_RT_REG_SZ, 0, file, &size) != ERROR_SUCCESS)
    {
        return 0;
    }

    for (i = 0u; file[i]; i++)
    {
        rooted |= file[i] == L'\\' || file[i] == L':';
    }

    dir[0] = 0;

    if (!rooted && !GetWindowsDirectoryW(dir, PATH_UNITS))
    {
        return 0;
    }

    join(whole, dir, rooted ? L"" : L"\\Fonts\\", file);

    if (!WideCharToMultiByte(CP_UTF8, 0, whole, -1, path, (int)path_max, 0, 0))
    {
        path[0] = 0;
    }

    return 0;
}
