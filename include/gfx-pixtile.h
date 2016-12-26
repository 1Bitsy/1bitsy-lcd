#ifndef GFX_PIXTILE_included
#define GFX_PIXTILE_included

#include <unistd.h>

#include <gfx-types.h>

struct gfx_pixtile {
    gfx_rgb565 *pixels;         // N.B., points to (0, 0) which may not exist.
    int         x, y;           // origin
    size_t      w, h;           // size
    ssize_t     stride;         // row stride in pixels
};

extern void gfx_init_pixtile(gfx_pixtile *tile,
                             void *buffer,
                             int x, int y,
                             size_t w, size_t h,
                             size_t stride);
                         
static inline gfx_rgb565 *gfx_pixel_address_unchecked(gfx_pixtile *tile,
                                                      int x, int y)
{
    return tile->pixels + y * tile->stride + x;
}

extern gfx_rgb565 *gfx_pixel_address(gfx_pixtile *tile, int x, int y);

// Copy source pixmap into destination.
// offset translates src coordinates to dest.
// dest coord = src coord + offset.
extern void gfx_copy_pixtile(gfx_pixtile       *dest,
                             gfx_pixtile const *src,
                             gfx_ipoint         offset);

#endif /* !GFX_PIXTILE_included */
