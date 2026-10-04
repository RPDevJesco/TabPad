#include "port_hosted.h"
#include "ed.h"
#include "hl.h"
#include "sess.h"
#include <stdio.h>
#include <stdlib.h>

uint32_t hosted_port_live;
uint32_t hosted_port_allocs;

static uint32_t fail_in;

void hosted_port_fail_alloc(uint32_t nth)
{
    fail_in = nth;
}

static void *alloc(size_t n)
{
    void *p;

    hosted_port_allocs++;

    if (fail_in && !--fail_in)
    {
        return 0;
    }

    p = malloc(n ? n : 1u);

    if (p)
    {
        hosted_port_live++;
    }

    return p;
}

static void release(void *p)
{
    if (!p)
    {
        return;
    }

    hosted_port_live--;
    free(p);
}

void *hl_mem_alloc(size_t n)
{
    return alloc(n);
}

void hl_mem_free(void *p)
{
    release(p);
}

void hl_panic(void)
{
    fputs("hl: the parser ran out of memory\n", stderr);
    abort();
}

void *sess_mem_alloc(size_t n)
{
    return alloc(n);
}

void sess_mem_free(void *p)
{
    release(p);
}

void *ed_mem_alloc(size_t n)
{
    return alloc(n);
}

void ed_mem_free(void *p)
{
    release(p);
}
