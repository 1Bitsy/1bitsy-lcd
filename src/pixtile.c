#include <gfx-pixtile.h>

extern void init_pixtile(gfx_pixtile *tile,
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
