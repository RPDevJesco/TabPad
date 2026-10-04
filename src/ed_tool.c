#include "ed_int.h"

#define INSET ed_px(4)
#define GROUP_GAP ed_px(9)

struct button {
    uint32_t icon, cmd;
};

static const struct button buttons[] = {
    { ED_I_NEW, ED_CMD_NEW },
    { ED_I_OPEN, ED_CMD_OPEN },
    { ED_I_SAVE, ED_CMD_SAVE },
    { ED_I_SAVE_ALL, ED_CMD_SAVE_ALL },
    { ED_I_CLOSE, ED_CMD_CLOSE },
    { ED_I_CLOSE_ALL, ED_CMD_CLOSE_ALL },
    { ED_NONE, 0u },
    { ED_I_CUT, ED_CMD_CUT },
    { ED_I_COPY, ED_CMD_COPY },
    { ED_I_PASTE, ED_CMD_PASTE },
    { ED_NONE, 0u },
    { ED_I_UNDO, ED_CMD_UNDO },
    { ED_I_REDO, ED_CMD_REDO },
    { ED_NONE, 0u },
    { ED_I_FIND, ED_CMD_FIND },
    { ED_I_REPLACE, ED_CMD_REPLACE },
    { ED_NONE, 0u },
    { ED_I_ZOOM_IN, ED_CMD_ZOOM_IN },
    { ED_I_ZOOM_OUT, ED_CMD_ZOOM_OUT },
    { ED_NONE, 0u },
    { ED_I_SPACES, ED_CMD_SPACES },
    { ED_I_WRAP, ED_CMD_WRAP },
};

#define BUTTONS (sizeof buttons / sizeof buttons[0])

static uint32_t hover = ED_NONE;

static int32_t side(void)
{
    return ed_g.icon + INSET * 2;
}

static int32_t button_x(uint32_t i)
{
    int32_t x = ED_PAD;
    uint32_t k;

    for (k = 0u; k < i; k++)
    {
        x += buttons[k].icon == ED_NONE ? GROUP_GAP : side() + ed_px(2);
    }

    return x;
}

static uint32_t button_at(int32_t x, int32_t y)
{
    uint32_t i;

    for (i = 0u; i < BUTTONS && y >= ed_g.tool_y + ed_px(2) && y < ed_g.tool_y + ed_g.tool_h - ed_px(2); i++)
    {
        if (buttons[i].icon != ED_NONE && x >= button_x(i) && x < button_x(i) + side())
        {
            return i;
        }
    }

    return ED_NONE;
}

static void draw_icon(uint32_t icon, int32_t x, int32_t y, uint32_t rgb)
{
    const char *row;
    int32_t r;
    int32_t c;
    int32_t from;

    for (r = 0; r < ED_ICON; r++)
    {
        row = ed_icons[icon][r];

        for (c = 0; c < ED_ICON; c++)
        {
            if (row[c] == '.')
            {
                continue;
            }

            for (from = c; c + 1 < ED_ICON && row[c + 1] != '.'; c++)
            {
            }

            ed_draw_rect(x + from * ed_g.icon / ED_ICON, y + r * ed_g.icon / ED_ICON, (c - from) * ed_g.icon / ED_ICON + ed_g.stroke, ed_g.stroke, rgb);
        }
    }
}

int ed_tool_down(int32_t x, int32_t y)
{
    uint32_t i = button_at(x, y);

    if (y < ed_g.tool_y || y >= ed_g.tool_y + ed_g.tool_h)
    {
        return 0;
    }

    if (i != ED_NONE)
    {
        ed_run(buttons[i].cmd, 0u);
    }

    return 1;
}

void ed_tool_move(int32_t x, int32_t y)
{
    uint32_t was = hover;

    hover = button_at(x, y);

    if (hover != was)
    {
        ed_touch();
    }
}

void ed_tool_draw(void)
{
    uint32_t i;
    int32_t x;
    int32_t y = ed_g.tool_y + (ed_g.tool_h - side()) / 2;
    int can;

    if (!ed_g.tool_h)
    {
        return;
    }

    ed_draw_clip(0, ed_g.tool_y, ed_w, ed_g.tool_h);
    ed_draw_rect(0, ed_g.tool_y, ed_w, ed_g.tool_h, ed_colours[ED_C_MENU_BACK]);

    for (i = 0u; i < BUTTONS; i++)
    {
        x = button_x(i);

        if (buttons[i].icon == ED_NONE)
        {
            ed_draw_rect(x + GROUP_GAP / 2 - 1, y + ed_px(2), 1, side() - ed_px(4), ed_colours[ED_C_BORDER]);
            continue;
        }

        can = ed_can(buttons[i].cmd, 0u);

        if ((i == hover && can) || ed_on(buttons[i].cmd, 0u))
        {
            ed_draw_rect(x, y, side(), side(), ed_colours[ED_C_MENU_HOT]);
        }

        draw_icon(buttons[i].icon, x + INSET, y + INSET, ed_colours[can ? ED_C_MENU_TEXT : ED_C_DIM]);
    }
}
