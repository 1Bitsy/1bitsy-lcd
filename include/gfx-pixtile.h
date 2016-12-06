#ifndef GFX_PIXTILE_included
#define GFX_PIXTILE_included

#include <gfx-types.h>

typedef struct gfx_pixtile {
    gfx_rgb565 *pixels;         // N.B., points to (0, 0) which may not exist.
    int         x, y;           // origin
    size_t      w, h;           // size
    ssize_t     stride;
} gfx_pixtile;

extern void init_pixtile(gfx_pixtile *tile,
                         void *buffer,
                         int x, int y,
                         size_t w, size_t h,
                         size_t stride);
                         
#endif /* !GFX_PIXTILE_included */
