#ifndef HL_SHIM_ENDIAN_H
#define HL_SHIM_ENDIAN_H

#include <stdint.h>

static inline uint16_t hl_c_le16(uint16_t x)
{
    const uint8_t *p = (const uint8_t *)&x;

    return (uint16_t)(p[0] | p[1] << 8);
}

static inline uint16_t hl_c_be16(uint16_t x)
{
    const uint8_t *p = (const uint8_t *)&x;

    return (uint16_t)(p[0] << 8 | p[1]);
}

#define le16toh(x) hl_c_le16(x)
#define be16toh(x) hl_c_be16(x)

#endif
