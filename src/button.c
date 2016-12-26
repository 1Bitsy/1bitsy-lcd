#include "gfx-button.h"

void gfx_button_init(gfx_button *button,
                     bool is_down,
                     gfx_ipoint position,
                     const gfx_pixtile *up_image,
                     const gfx_pixtile *down_image)
{
    button->is_down    = is_down;
    button->position   = position;
    button->up_image   = up_image;
    button->down_image = down_image;
}

bool gfx_point_is_in_button(const gfx_ipoint *pt, const gfx_button *btn)
{
    int x = pt->x - btn->position.x;
    int y = pt->y - btn->position.y;
    const gfx_pixtile *img = btn->up_image;
    if (x < img->x || x >= (int)(img->x + img->w))
        return false;
    if (y < img->y || y >= (int)(img->y + img->h))
        return false;
    return true;
}

void gfx_draw_button(gfx_pixtile *tile, const gfx_button *btn)
{
    const gfx_pixtile *src = btn->is_down ? btn->down_image : btn->up_image;
    gfx_copy_pixtile(tile, src, btn->position);
}
