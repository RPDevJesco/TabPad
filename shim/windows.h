#ifndef HL_SHIM_WINDOWS_H
#define HL_SHIM_WINDOWS_H

#include <stdint.h>

typedef void *HANDLE;

#define FALSE 0
#define DUPLICATE_SAME_ACCESS 2
#define GetCurrentProcess() ((HANDLE)0)
#define DuplicateHandle(sp, sh, tp, th, access, inherit, options) (*(th) = (HANDLE)0, 0)

static inline long InterlockedIncrement(long volatile *p)
{
    *p += 1;

    return *p;
}

static inline long InterlockedDecrement(long volatile *p)
{
    *p -= 1;

    return *p;
}

#endif
