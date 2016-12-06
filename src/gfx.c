#include <gfx.h>

#include <assert.h>
#include <stdbool.h>

#include <gfx-pixtile.h>

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

        if (x1 < x0) {
            float xt = x0; x0 = x1; x1 = xt;
            float yt = y0; y0 = y1; y1 = yt;
        }
        float dx = x1 - x0;
        float dy = ABS(y1 - y0);
        float d = 2 * dy - dx;
        d -= FRAC(x0) * dy;
        d += FRAC(y0) * dx;
        float y = y0;
        float y_inc = (y1 > y0) ? +1.0f : -1.0f;
        for (int ix = FLOOR(x0); ix <= FLOOR(x1); ix++) {
            int iy = FLOOR(y);
            gfx_rgb565 *p = pixel_ptr(tile, ix, iy);
            if (p)
                *p = color;
            if (d >= 0) {
                y += y_inc;
                d -= dx;
            }
            d += dy;
        }

    } else {

        // Verticalish

        if (y1 < y0) {
            float xt = x0; x0 = x1; x1 = xt;
            float yt = y0; y0 = y1; y1 = yt;
        }
        float dy = y1 - y0;
        float dx = ABS(x1 - x0);
        float d = 2 * dx - dy;
        d -= FRAC(y0) * dx;
        d += FRAC(x0) * dy;
        float x = x0;
        float x_inc = (x1 > x0) ? +1.0f : -1.0f;
        for (int iy = FLOOR(y0); iy <= FLOOR(y1); iy++) {
            int ix = FLOOR(x);
            gfx_rgb565 *p = pixel_ptr(tile, ix, iy);
            if (p)
                *p = color;
            if (d >= 0) {
                x += x_inc;
                d -= dy;
            }
            d += dx;
        }            
    }
}

// Line, anti-aliased using Xiaolin Wu's algorithm
void gfx_draw_line_aa(gfx_pixtile *tile,
                      float x0, float y0,
                      float x1, float y1,
                      gfx_rgb888 color)
{
    // Cribbed directly from en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
    // XXX Could be optimized by clipping earlier.

    bool steep = ABS(y1 - y0) > ABS(x1 - x0);
    if (steep) {
        float t0 = x0; x0 = y0; y0 = t0;
        float t1 = x1; x1 = y1; y1 = t1;
    }
    if (x1 < x0) {
        float xt = x0; x0 = x1; x1 = xt;
        float yt = y0; y0 = y1; y1 = yt;
    }
    float dx = x1 - x0;
    float dy = y1 - y0;
    if (dx == 0)
        return;                 // x0 == x1, y0 == y1, ∴ no line
    float gradient = dy / dx;

    // handle first endpoint
    float xend = ROUND(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = 255.0f * RFRAC(x0 + 0.5f);
    int xpxl1 = xend;
    int ypxl1 = FLOOR(yend);
    if (steep) {
        gfx_rgb565 *p = pixel_ptr(tile, ypxl1, xpxl1);
        if (p)
            *p = blend_pixel(*p, color, RFRAC(yend) * xgap);
        p = pixel_ptr(tile, ypxl1 + 1, xpxl1);
        if (p)
            *p = blend_pixel(*p, color, FRAC(yend) * xgap);
    } else {
        gfx_rgb565 *p = pixel_ptr(tile, xpxl1, ypxl1);
        if (p)
            *p = blend_pixel(*p, color, RFRAC(yend) * xgap);
        p = pixel_ptr(tile, xpxl1, ypxl1 + 1);
        if (p)
            *p = blend_pixel(*p, color, FRAC(yend) * xgap);
    }
    float intery = yend + gradient; // first y-intersection for the main loop

    // handle second endpoint
    xend = ROUND(x1);
    yend = y1 + gradient * (xend - x1);
    xgap = 255.0f * FRAC(x1 + 0.5f);
    int xpxl2 = xend;
    int ypxl2 = FLOOR(yend);
    if (steep) {
        gfx_rgb565 *p = pixel_ptr(tile, ypxl2, xpxl2);
        if (p)
            *p = blend_pixel(*p, color, RFRAC(yend) * xgap);
        p = pixel_ptr(tile, ypxl2 + 1, xpxl2);
        if (p)
            *p = blend_pixel(*p, color, FRAC(yend) * xgap);
    } else {
        gfx_rgb565 *p = pixel_ptr(tile, xpxl2, ypxl2);
        if (p)
            *p = blend_pixel(*p, color, RFRAC(yend) * xgap);
        p = pixel_ptr(tile, xpxl2, ypxl2 + 1);
        if (p)
            *p = blend_pixel(*p, color, FRAC(yend) * xgap);
    }

    // main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            int y = FLOOR(intery);
            int f = 256.0f * FRAC(intery);
            gfx_rgb565 *p = pixel_ptr(tile, y, x);
            if (p)
                *p = blend_pixel(*p, color, 255  - f);
            p = pixel_ptr(tile, y + 1, x);
            if (p)
                *p = blend_pixel(*p, color, f);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            int y = FLOOR(intery);
            int f = 256.0f * FRAC(intery);
            gfx_rgb565 *p = pixel_ptr(tile, x, y);
            if (p)
                *p = blend_pixel(*p, color, 255 - f);
            p = pixel_ptr(tile, x, y + 1);
            if (p)
                *p = blend_pixel(*p, color, f);
            intery += gradient;
        }
    }
}
