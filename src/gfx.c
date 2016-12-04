#include <gfx.h>

#include <assert.h>

#include "util.h"

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

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -
// Lines


// Bresenham, jaggy.
void gfx_draw_line(gfx_pixtile *tile,
                   float x0, float y0,
                   float x1, float y1,
                   gfx_rgb888 color)
{
    int min_x = tile->x;
    int max_x = min_x + tile->w;
    int min_y = tile->y;
    int max_y = min_y + tile->h;

    if (y0 == y1) {

        // Horizontal

        gfx_fill_span(tile, (int)x0, (int)x1, (int)y0, color);

    } else if (x0 == x1) {

        // Vertical

        int x = (int)x0;
        if (min_x <= x && x < max_x) {
            int first_y = MAX(min_y,     (int)MIN(y0, y1));
            int last_y  = MIN(max_y - 1, (int)MAX(y0, y1));
            for (int y = first_y; y <= last_y; y++)
                *pixel_ptr_unclipped(tile, x, y) = color;
        }

    } else if (ABS(x1 - x0) >= ABS(y1 - y0)) {

        // Horizontalish

#if 0
        if (x1 < x0) {
            coord xt = x0; x0 = x1; x1 = xt;
            coord yt = y0; y0 = y1; y1 = yt;
        }
        coord dx = x1 - x0;
        coord dy = ABS(y1 - y0);
        coord d = 2 * dy - dx;
        d -= coord_product(coord_frac(x0), dy);
        d += coord_product(coord_frac(y0), dx);
        coord y = y0;
        coord y_inc = int_to_coord((y1 > y0) ? +1 : -1);
        for (int ix = coord_to_int(x0); ix <= coord_to_int(x1); ix++) {
            int iy = coord_to_int(y);
            if (ix >= min_x && ix < max_x && iy >= min_y && iy < max_y)
                tile->pixels[iy - min_y][ix] = color;
            if (d >= 0) {
                y += y_inc;
                d -= dx;
            }
            d += dy;
        }
#endif

    } else {

        // Verticalish

#if 0
        if (y1 < y0) {
           coord xt = x0; x0 = x1; x1 = xt;
            coord yt = y0; y0 = y1; y1 = yt;
        }
        coord dy = y1 - y0;
        coord dx = ABS(x1 - x0);
        coord d = 2 * dx - dy;
        d -= coord_product(coord_frac(y0), dx);
        d += coord_product(coord_frac(x0), dy);
        coord x = x0;
        coord x_inc = int_to_coord((x1 > x0) ? +1 : -1);
        for (int y = coord_to_int(y0); y <= coord_to_int(y1); y++) {
            int ix = coord_to_int(x);
            if (y >= min_y && y < max_y && ix >= min_x && ix < max_x)
                tile->pixels[y - min_y][ix] = color;
            if (d >= 0) {
                x += x_inc;
                d -= dy;
            }
            d += dx;
        }            
#endif
    }
}
