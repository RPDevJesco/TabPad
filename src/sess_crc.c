#include "sess.h"

static const uint32_t nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};

uint32_t sess_crc(uint32_t crc, const void *buf, uint32_t n)
{
    const uint8_t *p = buf;
    uint32_t i;

    crc = ~crc;

    for (i = 0u; i < n; i++)
    {
        crc ^= p[i];
        crc = nibble[crc & 15u] ^ (crc >> 4);
        crc = nibble[crc & 15u] ^ (crc >> 4);
    }

    return ~crc;
}
