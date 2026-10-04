/* a sum */
#include <stdint.h>

static uint32_t sum(const uint32_t *v, uint32_t n)
{
    uint32_t i;
    uint32_t total = 0u;

    for (i = 0u; i < n; i++)
    {
        total += v[i];
    }

    return total;
}
