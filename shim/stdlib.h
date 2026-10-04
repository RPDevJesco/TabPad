#ifndef HL_SHIM_STDLIB_H
#define HL_SHIM_STDLIB_H

#include "hl_c.h"

#define malloc hl_c_malloc
#define calloc hl_c_calloc
#define realloc hl_c_realloc
#define free hl_c_free
#define abort hl_c_abort

#endif
