#ifndef STORE_H
#define STORE_H

#include <stdint.h>

extern uint32_t store_puts;
extern uint32_t store_dels;
extern uint32_t store_put_bytes;

int store_dir(const char *path);
void store_fail(uint32_t nth);
void store_crash(uint32_t nth, uint32_t bytes);

#endif
