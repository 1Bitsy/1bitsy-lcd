#include <gfx-pixtile.h>

#include <assert.h>

extern void pixtile_init(gfx_pixtile *tile,
                         void *buffer,
                         int x, int y,
                         size_t w, size_t h,
                         size_t stride)
{
    tile->pixels = (gfx_rgb565 *)buffer - y * stride - x;
    tile->x      = x;
    tile->y      = y;
    tile->w      = w;
    tile->h      = h;
    tile->stride = stride;
}

extern gfx_rgb565 *pixtile_pixel_address(gfx_pixtile *tile, int x, int y)
{
    assert(tile->x <= x && x <= tile->x + tile->w);
    assert(tile->y <= y && y <= tile->y + tile->h);
    return pixtile_pixel_address_unchecked(tile, x, y);
}
