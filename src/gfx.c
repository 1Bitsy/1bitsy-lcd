#include <gfx.h>

#include <assert.h>

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -
// Pixels

static inline gfx_rgb565 *pixel_ptr_unclipped(gfx_pixtile *tile, int x, int y)
{
    x -= tile->x;
    y -= tile->y;
    return &tile->pixels[y * tile->stride + x];
}

static inline gfx_rgb565 *pixel_ptr(gfx_pixtile *tile, int x, int y)
{
    x -= tile->x;
    y -= tile->y;
    if ((unsigned)x >= tile->w || (unsigned)y >= tile->h)
        return NULL;
    return &tile->pixels[y * tile->stride + x];
}

static gfx_rgb565 blend_pixel(gfx_rgb565 dest, gfx_rgb888 src, gfx_alpha8 alpha)
{
    uint32_t dr5 = dest >> 11 & 0x1f;
    uint32_t dg5 = dest >>  5 & 0x3f;
    uint32_t db5 = dest >>  0 & 0x1f;
    uint32_t dr8 = dr5 << 3 | dr5 >> 2;
    uint32_t dg8 = dg5 << 2 | dg5 >> 4;
    uint32_t db8 = db5 << 3 | db5 >> 2;
    uint32_t sr8 = src >> 16 & 0xFF;
    uint32_t sg8 = src >>  8 & 0xFF;
    uint32_t sb8 = src >>  0 & 0xFF;
    dr8 += (sr8 - dr8) * (alpha * 0x8081) >> 23; // divide by 255
    dg8 += (sg8 - dg8) * (alpha * 0x8081) >> 23;
    db8 += (sb8 - db8) * (alpha * 0x8081) >> 23;
    return (dr8 << 8 & 0xf800) | (dg8 << 3 & 0x07e0) | (db8 >> 3 & 0x001f);
}

void gfx_fill_pixel(gfx_pixtile *tile,
                    int x, int y,
                    gfx_rgb888 color)
{
    gfx_rgb565 *p = pixel_ptr(tile, x, y);
    if (p)
        *p = color;
}

void gfx_fill_pixel_blend(gfx_pixtile *tile,
                          int x, int y,
                          gfx_rgb888 color,
                          gfx_alpha8 alpha)
{
    if (!alpha)
        return;
    gfx_rgb565 *p = pixel_ptr(tile, x, y);
    if (p) {
        if (alpha == 0xFF)
            *p = color;
        else
            *p = blend_pixel(*p, color, alpha);
    }
}

void gfx_fill_pixel_unclipped(gfx_pixtile *tile,
                              int x, int y,
                              gfx_rgb888 color)
{
    gfx_rgb565 *p = pixel_ptr_unclipped(tile, x, y);
    *p = color;
}

void gfx_fill_pixel_blend_unclipped(gfx_pixtile *tile,
                                    int x, int y,
                                    gfx_rgb888 color,
                                    gfx_alpha8 alpha)
{
    gfx_rgb565 *p = pixel_ptr_unclipped(tile, x, y);
    *p = blend_pixel(*p, color, alpha);
}

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -
// Pixels

static gfx_rgb565 *span_clip(gfx_pixtile *tile,
                             int x0, int x1, int y,
                             size_t *count_out)
{
    if (x0 < tile->x)
        x0 = tile->x;
    if (x1 > tile->x + tile->w)
        x1 = tile->x + tile->w;
    if (x0 >= x1)
        return NULL;
    *count_out = x1 - x0;
    return pixel_ptr_unclipped(tile, x0, y);
}

void gfx_fill_span(gfx_pixtile *tile,
                   int x0, int x1, int y,
                   gfx_rgb888 color)
{
    size_t count;
    gfx_rgb565 *p = span_clip(tile, x0, x1, y, &count);
    for (size_t i = 0; i < count; i++)
        *p = color;
}

void gfx_fill_span_blend(gfx_pixtile *tile,
                         int x0, int x1, int y,
                         gfx_rgb888 color,
                         gfx_alpha8 alpha)
{
    if (alpha == 0)
        return;
    size_t count;
    gfx_rgb565 *p = span_clip(tile, x0, x1, y, &count);
    if (!p)
        return;
    if (alpha == 0xFF) {
        for (size_t i = 0; i < count; i++)
            *p++ = color;
    } else {
        for (size_t i = 0; i < count; i++) {
            *p = blend_pixel(*p, color, alpha);
            p++;
        }
    }
}

void gfx_fill_span_unclipped(gfx_pixtile *tile,
                             int x0, int x1, int y,
                             gfx_rgb888 color)
{
    gfx_rgb565 *p = pixel_ptr_unclipped(tile, x0, y);
    for (int i = x0; i < x1; i++)
        *p++ = color;
}

void gfx_fill_span_blend_unclipped(gfx_pixtile *tile,
                                   int x0, int x1, int y,
                                   gfx_rgb888 color,
                                   gfx_alpha8 alpha)
{
    gfx_rgb565 *p = pixel_ptr_unclipped(tile, x0, y);
    for (int i = x0; i < x1; i++) {
        *p = blend_pixel(*p, color, alpha);
        p++;
    }
}

