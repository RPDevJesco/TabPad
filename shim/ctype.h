#ifndef HL_SHIM_CTYPE_H
#define HL_SHIM_CTYPE_H

#include "hl_c.h"

#define isprint hl_c_isprint
#define isspace(c) hl_c_isspace((uint32_t)(c))
#define isalpha(c) hl_c_isalpha((uint32_t)(c))
#define isdigit(c) hl_c_isdigit((uint32_t)(c))
#define isalnum(c) hl_c_isalnum((uint32_t)(c))
#define isxdigit(c) hl_c_isxdigit((uint32_t)(c))
#define isupper(c) hl_c_isupper((uint32_t)(c))
#define islower(c) hl_c_islower((uint32_t)(c))
#define ispunct(c) hl_c_ispunct((uint32_t)(c))
#define toupper(c) ((int)hl_c_toupper((uint32_t)(c)))
#define tolower(c) ((int)hl_c_tolower((uint32_t)(c)))

#endif
