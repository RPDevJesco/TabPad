#include "ed_int.h"

#define STEPS_FIRST 32u
#define SPARE 32u

static void drop_above(struct ed_undo *u)
{
    while (u->n > u->pos)
    {
        ed_mem_free(u->steps[--u->n].text);
    }
}

struct ed_step *ed_undo_push(struct ed_undo *u, uint32_t removed, uint32_t inserted)
{
    struct ed_step *grown;
    struct ed_step *s;
    uint32_t max;
    uint32_t i;

    drop_above(u);
    u->typing = 0;

    if (u->n == u->max)
    {
        max = u->max ? u->max * 2u : STEPS_FIRST;
        grown = ed_mem_alloc((size_t)max * sizeof *grown);

        if (!grown)
        {
            return 0;
        }

        for (i = 0u; i < u->n; i++)
        {
            grown[i] = u->steps[i];
        }

        ed_mem_free(u->steps);
        u->steps = grown;
        u->max = max;
    }

    s = &u->steps[u->n];
    s->room = removed + inserted + SPARE;
    s->text = ed_mem_alloc((size_t)s->room * sizeof *s->text);

    if (!s->text)
    {
        return 0;
    }

    s->removed = removed;
    s->inserted = inserted;
    u->n++;
    u->pos = u->n;

    return s;
}

struct ed_step *ed_undo_join(struct ed_undo *u, uint32_t at, uint16_t unit)
{
    struct ed_step *s;
    uint16_t *grown;
    uint32_t used;
    uint32_t i;

    if (!u->typing || u->pos != u->n || !u->n)
    {
        return 0;
    }

    s = &u->steps[u->n - 1u];
    used = s->removed + s->inserted;

    if (at != s->at + s->inserted || unit == '\n')
    {
        return 0;
    }

    if (used == s->room)
    {
        grown = ed_mem_alloc((size_t)s->room * 2u * sizeof *grown);

        if (!grown)
        {
            return 0;
        }

        for (i = 0u; i < used; i++)
        {
            grown[i] = s->text[i];
        }

        ed_mem_free(s->text);
        s->text = grown;
        s->room *= 2u;
    }

    s->text[used] = unit;
    s->inserted++;

    return s;
}

void ed_undo_pop(struct ed_undo *u)
{
    ed_mem_free(u->steps[--u->n].text);
    u->pos = u->n;
    u->typing = 0;
}

void ed_undo_free(struct ed_undo *u)
{
    u->pos = 0u;
    drop_above(u);
    ed_mem_free(u->steps);
    u->steps = 0;
    u->max = 0u;
    u->typing = 0;
}
