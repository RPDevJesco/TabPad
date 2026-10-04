#ifndef HL_SHIM_WCTYPE_H
#define HL_SHIM_WCTYPE_H

#include "hl_c.h"

#define iswspace(c) hl_c_isspace((uint32_t)(c))
#define iswalpha(c) hl_c_isalpha((uint32_t)(c))
#define iswdigit(c) hl_c_isdigit((uint32_t)(c))
#define iswalnum(c) hl_c_isalnum((uint32_t)(c))
#define iswxdigit(c) hl_c_isxdigit((uint32_t)(c))
#define iswupper(c) hl_c_isupper((uint32_t)(c))
#define iswlower(c) hl_c_islower((uint32_t)(c))
#define iswpunct(c) hl_c_ispunct((uint32_t)(c))
#define towupper(c) hl_c_toupper((uint32_t)(c))
#define towlower(c) hl_c_tolower((uint32_t)(c))

#endif
