#include <gfx-pixtile.h>

extern void gfx_init_pixtile(gfx_pixtile *tile,
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

extern gfx_rgb565 *gfx_pixel_address(gfx_pixtile *tile, int x, int y)
{
    if (x < tile->x || x >= (ssize_t)(tile->x + tile->w))
        return NULL;
    if (y < tile->y || y >= (ssize_t)(tile->y + tile->h))
        return NULL;
    return gfx_pixel_address_unchecked(tile, x, y);
}
