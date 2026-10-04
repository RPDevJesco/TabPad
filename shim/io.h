#ifndef HL_SHIM_IO_H
#define HL_SHIM_IO_H

#include <stdint.h>

#define _open_osfhandle(h, flags) (-1)
#define _get_osfhandle(fd) ((intptr_t)-1)

#endif
