#ifndef SESS_FORMAT_H
#define SESS_FORMAT_H

#include <stdint.h>

#define SESS_VERSION 1u
#define SESS_M_MAGIC 0
#define SESS_M_VERSION 8
#define SESS_M_SEQ 12
#define SESS_M_TABS 16
#define SESS_M_ACTIVE 20
#define SESS_M_NEXT_ID 24
#define SESS_M_BYTES 28
#define SESS_M_SIZE 32
#define SESS_T_BACKUP 0
#define SESS_T_BACKUP_LEN 4
#define SESS_T_BACKUP_CRC 8
#define SESS_T_PATH_LEN 12
#define SESS_T_CARET 16
#define SESS_T_ANCHOR 20
#define SESS_T_SCROLL_ROW 24
#define SESS_T_SCROLL_COL 28
#define SESS_T_ENCODING 32
#define SESS_T_EOL 36
#define SESS_T_DISK_SIZE 40
#define SESS_T_DISK_MTIME 48
#define SESS_T_DISK_CRC 56
#define SESS_T_USER 60
#define SESS_T_LANG 64
#define SESS_T_SIZE 80
#define SESS_LANG_BYTES 16
#define SESS_B_MAGIC 0
#define SESS_B_VERSION 8
#define SESS_B_ID 12
#define SESS_B_LEN 16
#define SESS_B_RESERVED 20
#define SESS_B_SIZE 24

static inline uint32_t sess_ld16(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static inline uint32_t sess_ld32(const uint8_t *p)
{
    return sess_ld16(p) | sess_ld16(p + 2) << 16;
}

static inline uint64_t sess_ld64(const uint8_t *p)
{
    return (uint64_t)sess_ld32(p) | (uint64_t)sess_ld32(p + 4) << 32;
}

static inline void sess_st16(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
}

static inline void sess_st32(uint8_t *p, uint32_t v)
{
    sess_st16(p, v);
    sess_st16(p + 2, v >> 16);
}

static inline void sess_st64(uint8_t *p, uint64_t v)
{
    sess_st32(p, (uint32_t)v);
    sess_st32(p + 4, (uint32_t)(v >> 32));
}

static inline int sess_magic_eq(const uint8_t *p, const char *magic)
{
    uint32_t i;

    for (i = 0u; i < 8u; i++)
    {
        if (p[i] != (uint8_t)magic[i])
        {
            return 0;
        }
    }

    return 1;
}

#endif