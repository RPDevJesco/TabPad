#ifndef HL_SHIM_STDIO_H
#define HL_SHIM_STDIO_H

#include "hl_c.h"

#define stderr ((FILE *)0)

#define fprintf hl_c_fprintf
#define snprintf hl_c_snprintf
#define vsnprintf hl_c_vsnprintf
#define fputc hl_c_fputc
#define fputs hl_c_fputs
#define fclose hl_c_fclose
#define fdopen hl_c_fdopen
#define _fdopen hl_c_fdopen

#endif
