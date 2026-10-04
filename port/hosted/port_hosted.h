#ifndef PORT_HOSTED_H
#define PORT_HOSTED_H

#include <stdint.h>

extern uint32_t hosted_port_live;
extern uint32_t hosted_port_allocs;
void hosted_port_fail_alloc(uint32_t nth);

#endif
