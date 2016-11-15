#define MOVE
#define OUTER_CLIP

#include <assert.h>
#include <math.h>
#include <string.h>

#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include "coord.h"
#include "debounce.h"
#include "pixmaps.h"
#include "systick.h"
#include "util.h"
#include "video.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])


// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

// Verbs: draw, stroke, fill
// Objects: point, line, path, triangle, text
// Modifiers: aa, blended, noclip

// Colors: rgb888, rgb555, gray8

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

typedef uint16_t rgb565;
typedef uint32_t rgb888;        // 0x00rrggbb

typedef struct trapezoid {
    float xl0, xr0, y0;
    float xl1, xr1, y1;
} trapezoid;

typedef union point {
    struct {
        float x, y;             // use p.x, p.y
    };
    float c[2];                 // use p.c[i]
} point;

// Inputs: rotation, velocity, position
// Outputs: points, in_pts, angle, star_opacity
// Put those into an anim_state structure.

typedef struct anim_param {
} anim_param;

typedef struct anim_state {
    float radius;
    point vel;
    float angle;
    float rot_vel;
    point center;
    point points[5];
    point in_pts[5];
    float opacity;
    float opa_vel;
    int   opa_dir;
    
} anim_state;

static anim_state anim_states[6];

static inline rgb565 pack_color(rgb888 c)
{
    uint32_t r = (c >> 16 & 0xFF) >> 3;
    uint32_t g = (c >>  8 & 0xFF) >> 2;
    uint32_t b = (c >>  0 & 0xFF) >> 3;
    return r << 11 | g << 5 | b;
}

static inline rgb888 unpack_color(rgb565 c)
{
    uint32_t pr5 = c >> 11 & 0xF8;
    uint32_t pg5 = c >>  5 & 0xFC;
    uint32_t pb5 = c >>  0 & 0xF8;
    uint32_t pr8 = pr5 << 3 | pr5 >> 2;
    uint32_t pg8 = pg5 << 2 | pg5 >> 4;
    uint32_t pb8 = pb5 << 3 | pb5 >> 2;
    return pr8 << 16 | pg8 << 8 | pb8 << 0;
}

static inline rgb565 blend_to_black(rgb565 color, uint8_t alpha)
{
    uint32_t r5 = color >> 11 & 0x1f;
    uint32_t g6 = color >>  5 & 0x3f;
    uint32_t b5 = color >>  0 & 0x1f;
    // uint32_t r8 = r5 << 3 | r5 >> 2;
    // uint32_t g8 = g6 << 2 | g6 >> 4;
    // uint32_t b8 = b5 << 3 | b5 >> 2;
    // uint32_t r8 = r5 << 3;
    // uint32_t g8 = g6 << 2;
    // uint32_t b8 = b5 << 3;
    // r8 *= alpha; r8 >>= 8;
    // g8 *= alpha; g8 >>= 8;
    // b8 *= alpha; b8 >>= 8;
    r5 *= alpha; r5 >>= 5;
    g6 *= alpha; g6 >>= 6;
    b5 *= alpha; b5 >>= 5;
    return (r5 << 8 & 0xf800) | (g6 << 3 & 0x07e0) | (b5 >> 3 & 0x001f);
}

// XXX fast path when alpha == 0 or alpha == opaque.
static inline void blend_pixel_unclipped_co(pixtile *tile,
                                            int x, int y,
                                            rgb888 color, coord alpha)
{
    assert(0 <= alpha && alpha <= 256);

    rgb565 *p = &tile->pixels[y - tile->y][x];
    rgb565 pix = *p;
    uint32_t pr5 = pix >> 11 & 0x1f;
    uint32_t pg5 = pix >>  5 & 0x3f;
    uint32_t pb5 = pix >>  0 & 0x1f;
    uint32_t pr8 = pr5 << 3 | pr5 >> 2;
    uint32_t pg8 = pg5 << 2 | pg5 >> 4;
    uint32_t pb8 = pb5 << 3 | pb5 >> 2;
    uint32_t cr8 = color >> 16 & 0xFF;
    uint32_t cg8 = color >>  8 & 0xFF;
    uint32_t cb8 = color >>  0 & 0xFF;
    
    pr8 += coord_to_int((cr8 - pr8) * alpha);
    pg8 += coord_to_int((cg8 - pg8) * alpha);
    pb8 += coord_to_int((cb8 - pb8) * alpha);
    *p = (pr8 << 8 & 0xf800) | (pg8 << 3 & 0x07e0) | (pb8 >> 3 & 0x001f);
}

static inline void blend_pixel_co(pixtile *tile,
                               int x, int y,
                               rgb888 color, coord alpha)
{
    if (x_in_pixtile(tile, x) && y_in_pixtile(tile, y)) {
        blend_pixel_unclipped_co(tile, x, y, color, alpha);
    }
}

static void blend_pixel_span_unclipped(pixtile *tile,
                                       int x0, int x1, int y,
                                       rgb888 color,  uint8_t alpha)
{
    for (int x = x0; x <= x1; x++)
        blend_pixel_unclipped_co(tile, x, y, color, alpha);
}

static void draw_line_co(pixtile *tile,
                         coord x0, coord y0,
                         coord x1, coord y1,
                         uint16_t color)
{
    int min_x = 0;
    int max_x = PIXTILE_WIDTH;
    int min_y = tile->y;
    int max_y = min_y + tile->height;

    if (y0 == y1) {

        // Horizontal

        int y = coord_to_int(y0);
        if (min_y <= y && y < max_y) {
            int first_x = MAX(min_x, coord_to_int(MIN(x0, x1)));
            int last_x = MIN(max_x - 1, coord_to_int(MAX(x0, x1)));
            int yi = y - min_y;
            for (int x = first_x; x <= last_x; x++)
                tile->pixels[yi][x] = color;
        }

    } else if (x0 == x1) {

        // Vertical

        int x = coord_to_int(x0);
        if (min_x <= x && x < max_x) {
            int first_y = MAX(min_y, coord_to_int(MIN(y0, y1)));
            int last_y = MIN(max_y - 1, coord_to_int(MAX(y0, y1)));
            for (int y = first_y; y <= last_y; y++)
                tile->pixels[y - min_y][x] = color;
        }

    } else if (ABS(x1 - x0) >= ABS(y1 - y0)) {

        // Horizontalish

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

    } else {

        // Verticalish

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
    }
}

static void draw_line_aa_co(pixtile *tile,
                            coord x0, coord y0,
                            coord x1, coord y1,
                            rgb888 color,
                            uint8_t alpha)
{
    // Cribbed directly from en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm
    // Could be optimized by clipping earlier.

    bool steep = ABS(y1 - y0) > ABS(x1 - x0);
    if (steep) {
        coord t0 = x0; x0 = y0; y0 = t0;
        coord t1 = x1; x1 = y1; y1 = t1;
    }
    if (x1 < x0) {
        coord xt = x0; x0 = x1; x1 = xt;
        coord yt = y0; y0 = y1; y1 = yt;
    }
    coord dx = x1 - x0;
    coord dy = y1 - y0;
    float gradient = coord_to_float(dy) / coord_to_float(dx);

    // handle first endpoint
    coord xend = coord_round(x0);
    coord yend = float_to_coord(coord_to_float(y0) +
                                gradient * coord_to_float(xend - x0));
    coord xgap = coord_rfrac(x0 + float_to_coord(0.5));
    xgap = coord_product(int_to_coord(alpha), xgap);
    int xpxl1 = coord_to_int(xend);
    int ypxl1 = coord_to_int(yend);
    int a;
    if (steep) {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel_co(tile, ypxl1, xpxl1, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel_co(tile, ypxl1 + 1, xpxl1, color, a);
    } else {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel_co(tile, xpxl1, ypxl1, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel_co(tile, xpxl1, ypxl1 + 1, color, a);
    }
    float intery = coord_to_float(yend) + gradient;

    // handle second endpoint
    xend = coord_round(x1);
    yend = float_to_coord(coord_to_float(y1) +
                          gradient * coord_to_float(xend - x1));
    xgap = coord_frac(x1 + float_to_coord(0.5));
    xgap = coord_product(int_to_coord(alpha), xgap);
    int xpxl2 = coord_to_int(xend);
    int ypxl2 = coord_to_int(yend);
    if (steep) {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel_co(tile, ypxl2, xpxl2, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel_co(tile, ypxl2 + 1, xpxl2, color, a);
    } else {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel_co(tile, xpxl2, ypxl2, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel_co(tile, xpxl2, ypxl2 + 1, color, a);
    }

    // main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_rfrac(float_to_coord(intery))));
            blend_pixel_co(tile, (int)intery, x, color, a);
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_frac(float_to_coord(intery))));
            blend_pixel_co(tile, (int)intery + 1, x, color, a);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_rfrac(float_to_coord(intery))));
            blend_pixel_co(tile, x, (int)intery, color, a);
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_frac(float_to_coord(intery))));
            blend_pixel_co(tile, x, (int)intery + 1, color, a);
            intery += gradient;
        }
    }
}

#if 0
// project x onto the line (x0,y0) <-> (x1,y1) and return y.
static inline coord
project_x_to_line_co(coord x, coord x0, coord y0, coord x1, coord y1)
{
    if (x1 == x0)
        return y0;
    return y0 + coord_quotient(coord_product(x - x0, y1 - y0), x1 - x0);
}
#endif

#if 0
// project y onto the line (x0,y0) <-> (x1,y1) and return x.
static inline coord
project_y_to_line_co(coord y, coord x0, coord y0, coord x1, coord y1)
{
    if (y1 == y0)
        return x0;
    return x0 + coord_quotient(coord_product(y - y0, x1 - x0), y1 - y0);
}
#endif

static inline float
project_y_to_line(float y, float x0, float y0, float x1, float y1)
{
    if (y1 == y0)
        return x0;
    return x0 + (y - y0) * (x1 - x0) / (y1 - y0);
}

static inline void line_intersection(float x00, float y00, float x01, float y01,
                                     float x10, float y10, float x11, float y11,
                                     float *x_out, float *y_out)
{
    if (x01 == x00) {
        // line 0 is vertical.
        assert(x11 != x10);
        float m1 = (y11 - y10) / (x11 - x10);
        float b1 = y10 - x10 * m1;
        *x_out = x00;
        *y_out = m1 * x00 + b1;
    } else if (x11 == x10) {
        // line 1 is vertical.
        assert(x01 != x00);
        float m0 = (y01 - y00) / (x01 - x00);
        float b0 = y00 - x00 * m0;
        *x_out = x10;
        *y_out = m0 * x10 + b0;
    } else {
        float m0 = (y01 - y00) / (x01 - x00);
        float m1 = (y11 - y10) / (x11 - x10);
        assert(m0 != m1);
        float b0 = y01 - x01 * m0;
        float b1 = y11 - x11 * m1;
        float x = (b1 - b0) / (m0 - m1);
        *x_out = x;
        *y_out = m0 * x + b0;
    }    
}

static void fill_trapezoid_unclipped(pixtile *tile,
                                     const trapezoid *z,
                                     rgb888 color)
{
    float xl0 = z->xl0;
    float xr0 = z->xr0;
    float y0  = z->y0;
    float xl1 = z->xl1;
    float xr1 = z->xr1;
    float y1  = z->y1;

    float dy  = y1 - y0;
    float dxl = xl1 - xl0;
    float dxr = xr1 - xr0;
    int   iy0 = (int)y0;
    int   iy1 = (int)y1;

    for (int iy = iy0; iy < iy1; iy++) {
        int ixl = xl0 + (iy - iy0) * dxl / dy;
        int ixr = xr0 + (iy - iy0) * dxr / dy;
        for (int ix = ixl; ix < ixr; ix++)
            tile->pixels[iy - tile->y][ix] = color;
    }
}

static void fill_trapezoid_aa_unclipped(pixtile *tile,
                                        const trapezoid *z,
                                        rgb888 color, uint8_t alpha)
{
    const float xl0 = z->xl0;
    const float xr0 = z->xr0;
    const float y0 = z->y0;
    const float xl1 = z->xl1;
    const float xr1 = z->xr1;
    const float y1 = z->y1;
    
    const float dxl = xl1 - xl0;
    const float dxr = xr1 - xr0;
    const float dy = y1 - y0;
    assert(dy >= 0);
    int   incl = (dxl < 0) ? -1 : +1;
    int   incr = (dxr < 0) ? -1 : +1;
    // float half_incl = (dxl < 0) ? -0.5 : +0.5;
    // float half_incr = (dxr < 0) ? -0.5 : +0.5;
    int   comp_l    = (dxl < 0) ? ~0 : 0;
    int   comp_r    = (dxr < 0) ? ~0 : 0;

    const bool steepl = ABS(dxl) > dy;
    const bool steepr = ABS(dxr) > dy;
    int interp_l, gradient_l, alpha_l0;
    int interp_r, gradient_r, alpha_r0;
    if (steepl) {
        // X axis changes faster
        float m  = dy / dxl;
        interp_l   = (int)(65536.0 * (y0 + m * ((int)xl0 - xl0 + 0.5)));
        gradient_l = (int)(65536.0 * ABS(m));
        alpha_l0    = (interp_l ^ comp_l) >> 8 & 0xFF;
    } else {
        // Y axis changes faster.
        const float m = dxl / dy;
        interp_l   = (int)(65536.0 * (xl0 + m * ((int)y0 + 0.5 - y0)));
        gradient_l = (int)(65536.0 * dxl / dy);
        alpha_l0    = 0xFF - (interp_l >> 8 & 0xFF);
    }
    if (steepr) {
        // X axis changes faster
        float m  = dy / dxr;
        interp_r   = (int)(65536.0 * (y0 + m * ((int)xr0 - xr0 + 0.5)));
        gradient_r = (int)(65536.0 * ABS(m));
        alpha_r0    = 0xFF - ((interp_r ^ comp_r) >> 8 & 0xFF);
    } else {
        // Y axis changes faster.
        const float m = dxr / dy;
        interp_r   = (int)(65536.0 * (xr0 + m * ((int)y0 + 0.5 - y0)));
        gradient_r = (int)(65536.0 * dxr / dy);
        alpha_r0    = interp_r >> 8 & 0xFF;
    }
    int unfill_alpha = 0xFF * FRAC(y0);
    int fill_alpha   = 0xFF - unfill_alpha;
    int alpha_l        = MAX(0, alpha_l0 - unfill_alpha);
    int alpha_r        = MAX(0, alpha_r0 - unfill_alpha);

    const int ixl1 = (int)xl1;
    const int ixr1 = (int)xr1;
    const int iy1  = (int)(y1 - 0.0001);
    // const int cy1  = CEIL(y1);
    int       ixl  = (int)xl0;
    int       ixr  = (int)xr0;
    int       iy   = (int)y0;
    int       pixl, fixl;   // X coordinates of left pixel, left fill.
    int       pixr, fixr;   // X coordinates of right pixel, right fill.
    int       pal;          // left pixel alpha 0..255
    int       par;          // right pixel alpha 0..255

    while (iy <= iy1) {
        if (steepl) {
            int iyt = iy;
            fixl = ixl + 1;
            int fix2 = fixl;
            while (true) {
                pixl = ixl;
                fix2 = ixl + 1;
                pal = alpha_l;
                interp_l += gradient_l;
                alpha_l = (interp_l ^ comp_l) >> 8 & 0xFF;
                iyt = interp_l >> 16;
                ixl += incl;
                if (iyt != iy || incl * (ixl - ixl1) > comp_l)
                    break;
                blend_pixel_unclipped_co(tile, pixl, iyt, color, pal * alpha >> 8);
                // pixel(pixl, iyt, pal);
            }
            if (incl == +1)
                fixl = fix2;
        } else {
            pixl = ixl;
            fixl = ixl + 1;
            pal = alpha_l;
            interp_l += gradient_l;
            alpha_l = 0xFF - (interp_l >> 8 & 0xFF);
            ixl = interp_l >> 16;
        }
        if (steepr) {
            int iyt = iy;
            fixr = ixr - 1;
            int fix2 = fixr;
            while (true) {
                pixr = ixr;
                fix2 = ixr + 1;
                par = alpha_r;
                interp_r += gradient_r;
                alpha_r = 0xFF - ((interp_r ^ comp_r) >> 8 & 0xFF);
                iyt = interp_r >> 16;
                ixr += incr;
                if (iyt != iy || incr * (ixr - ixr1) > comp_r)
                    break;
                blend_pixel_unclipped_co(tile, pixr, iyt, color, par * alpha >> 8);
                // pixel(pixr, iyt, par);
            }
            if (incr == -1)
                fixr = fix2;
        } else {
            pixr = ixr;
            fixr = ixr - 1;
            par = alpha_r;
            interp_r += gradient_r;
            alpha_r = interp_r >> 8 & 0xFF;
            ixr = interp_r >> 16;
        }
        if (pixl == pixr) {
            // unfilled area = sum of unfilled areas.
            // (1 - a) = (1 - pal) - (1 - par)
            int a = MAX(0, pal + par - 0xFF);
            if (a > 0)
                blend_pixel_unclipped_co(tile, pixl, iy, color, a * alpha >> 8);
            
        } else if (pixl < pixr) {
            if (pal > 0)
                blend_pixel_unclipped_co(tile, pixl, iy, color, pal * alpha >> 8);
            if (fill_alpha > 0)
                blend_pixel_span_unclipped(tile,
                                           fixl, fixr, iy,
                                           color, fill_alpha * alpha >> 8);
            if (par > 0)
                blend_pixel_unclipped_co(tile, pixr, iy, color, par * alpha >> 8);
        }
        if (++iy == iy1) {
            // fill_alpha = 0xFF * (1 - FRAC(y1));
            fill_alpha = 0xFF * (y1 - iy1);
            if (xl1 != xr1)
                fill_alpha = 0xFF; // XXX hack!
            unfill_alpha = 0xFF - fill_alpha;
            alpha_l = MAX(0, alpha_l - unfill_alpha);

        } else {
            fill_alpha = 0xFF;
        }
    }
}

static float intersect_trapezoid_left_with_x(const trapezoid *z, float x)
{
    assert(z->xl0 != z->xl1);
    float m = (z->y1 - z->y0) / (z->xl1 - z->xl0);
    return z->y0 + m * (x - z->xl0);
}

static float intersect_trapezoid_right_with_x(const trapezoid *z, float x)
{
    assert(z->xr0 != z->xr1);
    float m = (z->y1 - z->y0) / (z->xr1 - z->xr0);
    return z->y0 + m * (x - z->xr0);
}

static float intersect_trapezoid_left_with_y(const trapezoid *z, float y)
{
    assert(z->y0 != z->y1);
    float m_inv = (z->xl1 - z->xl0) / (z->y1 - z->y0);
    return z->xl0 + m_inv * (y - z->y0);
}

static float intersect_trapezoid_right_with_y(const trapezoid *z, float y)
{
    assert(z->y0 != z->y1);
    float m_inv = (z->xr1 - z->xr0) / (z->y1 - z->y0);
    return z->xr0 + m_inv * (y - z->y0);
}

static size_t clip_trapezoids_min_y(trapezoid *zoids, size_t n, float min_y)
{
    // verify trapezoids are in order.
    for (size_t i = 1; i < n; i++)
        assert(zoids[i-1].y1 == zoids[i].y0);

    size_t i;
    for (i = 0; i < n; i++)
        if (zoids[i].y1 > min_y)
            break;

    // move the good ones up.
    if (i > 0 && i < n)
        memmove(zoids, zoids + i, (n - i) * sizeof *zoids);
    n -= i;

    // adjust the top edge
    if (n) {
        trapezoid *z = &zoids[0];
        if (z->y0 < min_y) {
            z->xl0 = intersect_trapezoid_left_with_y(z, min_y);
            z->xr0 = intersect_trapezoid_right_with_y(z, min_y);
            z->y0  = min_y;
        }
    }

    return n;
}

static size_t clip_trapezoids_max_y(trapezoid *zoids, size_t n, float max_y)
{
    // verify trapezoids are in order.
    for (size_t i = 1; i < n; i++)
        assert(zoids[i-1].y1 == zoids[i].y0);

   // forget the off-screen trapezoids.
    while (n > 0 && zoids[n-1].y0 >= max_y)
        n--;

    // adjust the bottom edge.
    if (n) {
        trapezoid *z = &zoids[n - 1]; // last one
        if (z->y1 > max_y) {
            z->xl1 = intersect_trapezoid_left_with_y(z, max_y);
            z->xr1 = intersect_trapezoid_right_with_y(z, max_y);
            z->y1  = max_y;
        }
    }

    return n;
}

static size_t clip_trapezoids_min_x(trapezoid *zoids, size_t n, float min_x)
{
    float nx, ny, ny1;
    trapezoid *nz;

    // verify trapezoids are left-to-right.
    for (size_t i = 0; i < n; i++) {
        trapezoid *z = &zoids[i];
        assert(z->xl0 <= z->xr0);
        assert(z->xl1 <= z->xr1);
    }

    for (size_t i = 0; i < n; i++) {
        trapezoid *z = &zoids[i];
        int mask = ((z->xl0 < min_x) << 0 |
                    (z->xl1 < min_x) << 1 |
                    (z->xr0 < min_x) << 2 |
                    (z->xr1 < min_x) << 3);
        switch (mask) {

        case 0:                 // not clipped
            break;

        case 1:                 // xl0 clipped: split.
            assert(n < 5);
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_left_with_x(z, min_x);
            nx      = intersect_trapezoid_right_with_y(z, ny);

            nz->y0  = ny;
            nz->xl0 = min_x;
            nz->xr0 = nx;
            nz->xl1 = z->xl1;
            nz->xr1 = z->xr1;
            nz->y1  = z->y1;

            z->xl0  = min_x;
            z->xl1  = min_x;
            z->xr1  = nx;
            z->y1   = ny;
            // z->xr0, y0 unchanged
            break;

        case 2:                 // xl1 clipped: split.
            assert(n < 5);
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_left_with_x(z, min_x);
            nx      = intersect_trapezoid_right_with_y(z, ny);

            // XXX due to rounding error, we can end up with nx < min_x.
            // XXX in that case, we should discard the trapezoid, I think.

            nz->xl0 = min_x;
            nz->xr0 = nx;
            nz->y0  = ny;
            nz->xl1 = min_x;
            nz->xr1 = z->xr1;
            nz->y1  = z->y1;

            z->xl1  = min_x;
            z->xr1  = nx;
            z->y1   = ny;
            // z->xl0, xr0, y0 unchanged
            break;

        case 3:                 // xl0 and xl1 clipped
            z->xl0  = min_x;
            z->xl1  = min_x;
            break;

        case 5:                 // xl0 and xr0 clipped: split
            assert(n < 5);
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_left_with_x(z, min_x);
            nx      = intersect_trapezoid_right_with_y(z, ny);
            ny1     = intersect_trapezoid_right_with_x(z, min_x);

            nz->xl0 = min_x;
            nz->xr0 = nx;
            nz->y0  = ny;
            nz->xl1 = z->xl1;
            nz->xr1 = z->xr1;
            nz->y1  = z->y1;

            z->xl0  = min_x;
            z->xr0  = min_x;
            z->y0   = ny1;
            z->xl1  = min_x;
            z->xr1  = nx;
            z->y1   = ny;
            break;

        case 7:                 // xl0, xr0, xl1 clipped
            ny      = intersect_trapezoid_right_with_x(z, min_x);

            z->y0   = ny;
            z->xl0  = min_x;
            z->xr0  = min_x;
            z->xl1  = min_x;
            // z->xr1, y1 unchanged
            break;

        case 10:                // xl1, xr1 clipped: split
            assert(n < 5);
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_left_with_x(z, min_x);
            nx      = intersect_trapezoid_right_with_y(z, ny);
            ny1     = intersect_trapezoid_right_with_x(z, min_x);

            nz->xl0 = min_x;
            nz->xr0 = nx;
            nz->y0  = ny;
            nz->xl1 = min_x;
            nz->xr1 = min_x;
            nz->y1  = ny1;

            z->xl1  = min_x;
            z->xr1  = nx;
            z->y1   = ny;
            // z->xl0, xl1, y0 unchanged
            break;

        case 11:                // xl0, xl1, xr1 clipped
            ny      = intersect_trapezoid_right_with_x(z, min_x);
            z->xl0  = min_x;
            z->xl1  = min_x;
            z->xr1  = min_x;
            z->y1   = ny;
            // z->xr0, y0 unchanged
            break;

        case 15:                // off screen
            zoids[i--] = zoids[--n];
            break;

        default:                // impossible
            assert(false);
        }
    }

    return n;
}

static size_t clip_trapezoids_max_x(trapezoid *zoids, size_t n, float max_x)
{
    float nx, ny, ny1;
    trapezoid *nz;

    // verify trapezoids are left-to-right.
    for (size_t i = 0; i < n; i++) {
        trapezoid *z = &zoids[i];
        assert(z->xl0 <= z->xr0);
        assert(z->xl1 <= z->xr1);
    }

    for (size_t i = 0; i < n; i++) {
        trapezoid *z = &zoids[i];
        int mask = ((z->xl0 > max_x) << 0 |
                    (z->xr0 > max_x) << 1 |
                    (z->xl1 > max_x) << 2 |
                    (z->xr1 > max_x) << 3);
        switch (mask) {

        case 0:                 // not clipped
            break;

        case 2:                 // pr0 clipped: split
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_right_with_x(z, max_x);
            nx      = intersect_trapezoid_left_with_y(z, ny);

            nz->xl0 = nx;
            nz->xr0 = max_x;
            nz->y0  = ny;
            nz->xl1 = z->xl1;
            nz->xr1 = z->xr1;
            nz->y1  = z->y1;

            z->xr0  = max_x;
            z->xl1  = nx;
            z->xr1  = max_x;
            z->y1   = ny;
            break;

        case 3:                 // pl0, pr0 clipped: split
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_right_with_x(z, max_x);
            nx      = intersect_trapezoid_left_with_y(z, ny);
            ny1     = intersect_trapezoid_left_with_x(z, max_x);

            nz->xl0 = nx;
            nz->xr0 = max_x;
            nz->y0  = ny;
            nz->xl1 = z->xl1;
            nz->xr1 = z->xr1;
            nz->y1  = z->y1;

            z->xl0  = max_x;
            z->xr0  = max_x;
            z->y0   = ny1;
            z->xl1  = nx;
            z->xr1  = max_x;
            z->y1   = ny;
            break;

        case 8:                 // pr1 clipped: split
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_right_with_x(z, max_x);
            nx      = intersect_trapezoid_left_with_y(z, ny);

            nz->xl0 = nx;
            nz->xr0 = max_x;
            nz->y0  = ny;
            nz->xl1 = z->xl1;
            nz->xr1 = max_x;
            nz->y1  = z->y1;

            z->xl1  = nx;
            z->xr1  = max_x;
            z->y1   = ny;
            break;

        case 10:                // pr0, pr1 clipped
            z->xr0 = max_x;
            z->xr1 = max_x;
            break;

        case 11:                // pl0, pr0, pr1 clipped
            ny     = intersect_trapezoid_left_with_x(z, max_x);

            z->xl0 = max_x;
            z->xr0 = max_x;
            z->y0  = ny;
            z->xr1 = max_x;
            break;

        case 12:                // pl1, pr1 clipped: split
            nz      = &zoids[n++];
            ny      = intersect_trapezoid_right_with_x(z, max_x);
            nx      = intersect_trapezoid_left_with_y(z, ny);
            ny1     = intersect_trapezoid_left_with_x(z, max_x);

            nz->xl0 = nx;
            nz->xr0 = max_x;
            nz->y0  = ny;
            nz->xl1 = max_x;
            nz->xr1 = max_x;
            nz->y1  = ny1;

            z->xl1  = nx;
            z->xr1  = max_x;
            z->y1   = ny;
            break;

        case 14:                // pr0, pl1, pr1 clipped
            ny     = intersect_trapezoid_left_with_x(z, max_x);

            z->xr0 = max_x;
            z->xl1 = max_x;
            z->xr1 = max_x;
            z->y1  = ny;
            break;

        case 15:                // off screen
            zoids[i--] = zoids[--n];
            break;

        default:                // impossible
            assert(false);
        }
    }

    return n;
}

static size_t clip_trapezoids(pixtile *tile, trapezoid z[5], size_t n)
{
    n = clip_trapezoids_max_y(z, n, tile->y + tile->height);
    n = clip_trapezoids_min_y(z, n, tile->y);
    n = clip_trapezoids_min_x(z, n, 0);
    n = clip_trapezoids_max_x(z, n, PIXTILE_WIDTH);
    return n;
}

static void fill_triangle(pixtile *tile, float verts[3][2], uint16_t color)
{
    float x0, y0, x1, y1, x2, y2;

    // sort vertices by y.
    if (verts[0][1] <= verts[1][1]) {
        if (verts[0][1] <= verts[2][1]) {
            if (verts[1][1] <= verts[2][1]) {
                // v0 <= v1 <= v2
                x0 = verts[0][0]; y0 = verts[0][1];
                x1 = verts[1][0]; y1 = verts[1][1];
                x2 = verts[2][0]; y2 = verts[2][1];
            } else {
                // v0 <= v2 < v1
                x0 = verts[0][0]; y0 = verts[0][1];
                x1 = verts[2][0]; y1 = verts[2][1];
                x2 = verts[1][0]; y2 = verts[1][1];
            }
        } else {
            // v2 < v0 < v1
            x0 = verts[2][0]; y0 = verts[2][1];
            x1 = verts[0][0]; y1 = verts[0][1];
            x2 = verts[1][0]; y2 = verts[1][1];
        }
    } else {
        // v1 < v0
        if (verts[2][1] <= verts[1][1]) {
            // v2 <= v1 < v0
            x0 = verts[2][0]; y0 = verts[2][1];
            x1 = verts[1][0]; y1 = verts[1][1];
            x2 = verts[0][0]; y2 = verts[0][1];
        } else {
            // v1 < v0, v1 < v2
            if (verts[0][1] <= verts[2][1]) {
                // v1 < v0 <= v2
                x0 = verts[1][0]; y0 = verts[1][1];
                x1 = verts[0][0]; y1 = verts[0][1];
                x2 = verts[2][0]; y2 = verts[2][1];
            } else {
                // v1 < v2 < v0
                x0 = verts[1][0]; y0 = verts[1][1];
                x1 = verts[2][0]; y1 = verts[2][1];
                x2 = verts[0][0]; y2 = verts[0][1];
            }
        }
    }

    float px1 = project_y_to_line(y1, x0, y0, x2, y2);

    float xl1 = (px1 < x1) ? px1 : x1;
    float xr1 = (px1 < x1) ? x1 : px1;

    trapezoid zoids[5];
    trapezoid *z = zoids;

    if (y0 != y1) {
        z->xl0 = x0;
        z->xr0 = x0;
        z->y0  = y0;
        z->xl1 = xl1;
        z->xr1 = xr1;
        z->y1  = y1;
        z++;
    }

    if (y1 != y2) {
        z->xl0 = xl1;
        z->xr0 = xr1;
        z->y0  = y1;
        z->xl1 = x2;
        z->xr1 = x2;
        z->y1  = y2;
        z++;
    }

    size_t n = z - zoids;
    n = clip_trapezoids(tile, zoids, n);
    for (size_t i = 0; i < n; i++)
        fill_trapezoid_unclipped(tile, &zoids[i], color);

    // fill_trapezoid(tile, x0, x0, y0, xl1, xr1, y1, color);
    // fill_trapezoid(tile, xl1, xr1, y1, x2, x2, y2, color);
}                          

static void fill_triangle_aa(pixtile *tile,
                             float verts[3][2],
                             rgb888 color, uint8_t alpha)
{
    float x0, y0, x1, y1, x2, y2;

    // sort vertices by y.
    if (verts[0][1] <= verts[1][1]) {
        if (verts[0][1] <= verts[2][1]) {
            if (verts[1][1] <= verts[2][1]) {
                // v0 <= v1 <= v2
                x0 = verts[0][0]; y0 = verts[0][1];
                x1 = verts[1][0]; y1 = verts[1][1];
                x2 = verts[2][0]; y2 = verts[2][1];
            } else {
                // v0 <= v2 < v1
                x0 = verts[0][0]; y0 = verts[0][1];
                x1 = verts[2][0]; y1 = verts[2][1];
                x2 = verts[1][0]; y2 = verts[1][1];
            }
        } else {
            // v2 < v0 < v1
            x0 = verts[2][0]; y0 = verts[2][1];
            x1 = verts[0][0]; y1 = verts[0][1];
            x2 = verts[1][0]; y2 = verts[1][1];
        }
    } else {
        // v1 < v0
        if (verts[2][1] <= verts[1][1]) {
            // v2 <= v1 < v0
            x0 = verts[2][0]; y0 = verts[2][1];
            x1 = verts[1][0]; y1 = verts[1][1];
            x2 = verts[0][0]; y2 = verts[0][1];
        } else {
            // v1 < v0, v1 < v2
            if (verts[0][1] <= verts[2][1]) {
                // v1 < v0 <= v2
                x0 = verts[1][0]; y0 = verts[1][1];
                x1 = verts[0][0]; y1 = verts[0][1];
                x2 = verts[2][0]; y2 = verts[2][1];
            } else {
                // v1 < v2 < v0
                x0 = verts[1][0]; y0 = verts[1][1];
                x1 = verts[2][0]; y1 = verts[2][1];
                x2 = verts[0][0]; y2 = verts[0][1];
            }
        }
    }

    float px1 = project_y_to_line(y1, x0, y0, x2, y2);

    float xl1 = (px1 < x1) ? px1 : x1;
    float xr1 = (px1 < x1) ? x1 : px1;

    trapezoid zoids[5];
    trapezoid *z = zoids;

    if (y0 != y1) {
        z->xl0 = x0;
        z->xr0 = x0;
        z->y0  = y0;
        z->xl1 = xl1;
        z->xr1 = xr1;
        z->y1  = y1;
        z++;
    }

    if (y1 != y2) {
        z->xl0 = xl1;
        z->xr0 = xr1;
        z->y0  = y1;
        z->xl1 = x2;
        z->xr1 = x2;
        z->y1  = y2;
        z++;
    }

    size_t n = z - zoids;
    n = clip_trapezoids(tile, zoids, n);
    for (size_t i = 0; i < n; i++) {
        z = &zoids[i];
        fill_trapezoid_aa_unclipped(tile, z, color, alpha);
    }
}


// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

#define ROTATION_RATE 0.005      // radians/frame
#define OPACITY_RATE  0.020      // alpha/frame
#define STAR_RADIUS   (0.44 * MIN(SCREEN_HEIGHT, SCREEN_WIDTH))
#define FG_COLOR      0xffff
#define BG_COLOR      0x0802
#define CENTER_X_MIN (STAR_RADIUS - 50)
#define CENTER_X_MAX (SCREEN_WIDTH - STAR_RADIUS + 50)
#define CENTER_Y_MIN (STAR_RADIUS - 50)
#define CENTER_Y_MAX (SCREEN_HEIGHT - STAR_RADIUS + 50)
#define INST_Y       (SCREEN_HEIGHT - 25)

typedef enum drawing_mode {
    MODE_OUTLINE,
    MODE_OUTLINE_AA,
    MODE_OUTLINE_AA_FADE,
    MODE_FILL,
    MODE_FILL_AA,
    MODE_FILL_AA_FADE,
    MODE_MORE,
    DRAWING_MODE_COUNT          // must be last
} drawing_mode;

static drawing_mode current_mode;
static drawing_mode new_mode;
static uint32_t mode_start_msec;

// static float angle = 0.2;
static float points[5][2];
static float in_pts[5][2];
// static coord center[2];
static int star_opacity = 0;


static void outline_star(pixtile *tile, rgb888 color)
{
    rgb565 pcolor = pack_color(color);
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord x0_co = float_to_coord(points[i][0]);
        coord y0_co = float_to_coord(points[i][1]);
        coord x1_co = float_to_coord(points[j][0]);
        coord y1_co = float_to_coord(points[j][1]);
        draw_line_co(tile, x0_co, y0_co, x1_co, y1_co, pcolor);
    }
}

static void outline_star_aa(pixtile *tile, rgb888 color, uint8_t alpha)
{
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord x0_co = float_to_coord(points[i][0]);
        coord y0_co = float_to_coord(points[i][1]);
        coord x1_co = float_to_coord(points[j][0]);
        coord y1_co = float_to_coord(points[j][1]);
        draw_line_aa_co(tile, x0_co, y0_co, x1_co, y1_co, color, alpha);
    }
}

static void fill_star(pixtile *tile, rgb888 color)
{
    rgb565 pcolor = pack_color(color);
    for (size_t i = 0; i < 5; i++) {
        size_t j = (i + 3) % 5;
        size_t k = (i + 4) % 5;
        float tri_pts[3][2] = {
            { points[i][0], points[i][1] },
            { in_pts[j][0], in_pts[j][1] },
            { in_pts[k][0], in_pts[k][1] },
        };
        fill_triangle(tile, tri_pts, pcolor);
    }
}

static void fill_star_aa(pixtile *tile, rgb888 color, uint8_t alpha)
{
    for (size_t i = 0; i < 5; i++) {
        size_t j = (i + 3) % 5;
        size_t k = (i + 4) % 5;
        float tri_pts[3][2] = {
            { points[i][0], points[i][1] },
            { in_pts[j][0], in_pts[j][1] },
            { in_pts[k][0], in_pts[k][1] },
        };
        fill_triangle_aa(tile, tri_pts, color, alpha);
    }
}

typedef struct mode_settings mode_settings;
typedef void draw_op(pixtile *, const mode_settings *);

typedef enum text_anim {
    TA_ALL,
    TA_DELAY,
    TA_FADE,
    TA_RAINBOW,
} text_anim;

typedef struct mode_settings {
    rgb888   bg_color;
    rgb888   fg_color;
    rgb888   tx_color;
    draw_op  *op;
    const text_pixmap *instructions;
    text_anim inst_anim;
} mode_settings;

static void draw_outline(pixtile *tile, const mode_settings *mode)
{
    outline_star(tile, mode->fg_color);
}

static void draw_outline_aa(pixtile *tile, const mode_settings *mode)
{
    outline_star_aa(tile, mode->fg_color, 0xFF);
}

static void draw_outline_fa(pixtile *tile, const mode_settings *mode)
{
    // int opacity = star_opacity * star_opacity / 255;
    int opacity = star_opacity;
    outline_star_aa(tile, mode->fg_color, opacity);
}

static void draw_filled(pixtile *tile, const mode_settings *mode)
{
    fill_star(tile, mode->fg_color);
}

static void draw_filled_aa(pixtile *tile, const mode_settings *mode)
{
    fill_star_aa(tile, mode->fg_color, 0xFF);
}

static void draw_filled_fa(pixtile *tile, const mode_settings *mode)
{
    // int opacity = star_opacity * star_opacity / 255;
    int opacity = star_opacity;
    fill_star_aa(tile, mode->fg_color, opacity);
}

static const mode_settings demo_modes[];

static void draw_background(pixtile *tile)
{
    static size_t bg_offset;
    static int alpha;
    static int inc = +1;

    if (tile->y == 0) {
        bg_offset = (bg_offset + 5) % BG_PIXMAP_WIDTH;
        alpha += inc;
        if (alpha > 0xFF) {
            alpha = 0x1fe - alpha;
            inc = -1;
        } else if (alpha < 0) {
            alpha = -alpha;
            inc = +1;
        }
    }

    size_t y0 = tile->y, y1 = y0 + tile->height;
    assert(y1 <= BG_PIXMAP_HEIGHT);

    size_t x0 = bg_offset, x1 = x0 + PIXTILE_WIDTH, x2 = x1;
    if (x1 > BG_PIXMAP_WIDTH) {
        x2 = x1 - BG_PIXMAP_WIDTH;
        x1 = BG_PIXMAP_WIDTH;
    }

    for (size_t y = y0, iy = 0; y < y1; y++, iy++)
        for (size_t x = x0, ix = 0; x < x1; x++, ix++)
            // tile->pixels[iy][ix] = bg_pixmap[y][x];
            tile->pixels[iy][ix] = blend_to_black(bg_pixmap[y][x], alpha);
    if (x2 != x1)
        for (size_t y = y0, iy = 0; y < y1; y++, iy++)
            for (size_t x = 0, ix = x1 - x0; x < x2; x++, ix++)
                // tile->pixels[iy][ix] = bg_pixmap[y][x];
                tile->pixels[iy][ix] = blend_to_black(bg_pixmap[y][x], alpha);
}

static void draw_everything(pixtile *tile, const mode_settings *mode)
{
    static const size_t op_permute[7] = { 3, 4, 5, 0, 1, 2, 6 };

    draw_background(tile);
    for (size_t i = 0; ; i++) {
        const mode_settings *mode = &demo_modes[op_permute[i]];
        if (mode->op == draw_everything)
            break;
        memcpy(points, anim_states[i].points, sizeof points);
        memcpy(in_pts, anim_states[i].in_pts, sizeof in_pts);
        star_opacity = 255 * anim_states[i].opacity;
        mode->op(tile, mode);
    }
}

static const mode_settings demo_modes[] = {
//    bg_color  fg_color  tx_color  op               inst      inst_anim
    { 0x000000, 0xff0000, 0xffff00, draw_outline,    &pb2aa,   TA_DELAY   },
    { 0x000018, 0x00ff00, 0xff00ff, draw_outline_aa, &pb2fade, TA_DELAY   },
    { 0xffffff, 0x0000ff, 0x003333, draw_outline_fa, &pb2fill, TA_FADE    },
    { 0x080018, 0xffff00, 0x00ff00, draw_filled,     &pb2aa,   TA_FADE    },
    { 0x002000, 0xff00ff, 0xffff00, draw_filled_aa,  &pb2fade, TA_FADE    },
    { 0x550000, 0x00ffff, 0x0000ff, draw_filled_fa,  &pb4more, TA_FADE    },
    { 0xffffff, 0x110000, 0x223322, draw_everything, &pb4less, TA_RAINBOW },
};
static const size_t mode_count = (&demo_modes)[1] - demo_modes;

static void init_anim_state(anim_state *a,
                            float radius,
                            point vel,
                            float rot_vel,
                            float opacity_vel)
{
    a->radius  = radius;
    a->vel     = vel;
    a->angle   = 0;
    a->rot_vel = rot_vel;
    a->center  = (point){ { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 } };
    a->opacity = 0;
    a->opa_vel = opacity_vel;
    a->opa_dir = +1;
}

static void setup_animation(void)
{
#ifdef MOVE
    point vel = { { +1, +1 } };
#else
    point vel = { { +0, +0 } };
#endif
    init_anim_state(&anim_states[0],
                    STAR_RADIUS,
                    vel,
                    ROTATION_RATE,
                    OPACITY_RATE);

    init_anim_state(&anim_states[1],
                    0.34 * SCREEN_WIDTH,
                    (point) { { 1.5, 1.5 } },
                    -0.003,
                    0.030);

    init_anim_state(&anim_states[2],
                    0.34 * SCREEN_WIDTH,
                    (point) { { 1.5, -1.5 } },
                    +0.004,
                    0.030);

    init_anim_state(&anim_states[3],
                    0.24 * SCREEN_WIDTH,
                    (point) { { -1, 0.5 } },
                    -0.008,
                    0.030);

    init_anim_state(&anim_states[4],
                    0.48 * SCREEN_WIDTH,
                    (point) { { -1, -1.5 } },
                    +0.002,
                    0.030);

    init_anim_state(&anim_states[5],
                    0.40 * SCREEN_WIDTH,
                    (point) { { 1.0, -1.1 } },
                    +0.003,
                    0.040);
}

static void anim_state_update(anim_state *a)
{
    // static const float past_edge = 40;
    #define PAST_EDGE 40
    static const point min_pos = { { -PAST_EDGE, -PAST_EDGE } };
    static const point max_pos = { { SCREEN_WIDTH  + PAST_EDGE,
                                     SCREEN_HEIGHT + PAST_EDGE } };

    // Update angle.
    a->angle += a->rot_vel;
    if (a->angle > 2 * M_PI)
        a->angle -= 2 * M_PI;
    else if (a->angle < 0)
        a->angle += 2 * M_PI;

    // Update position.
    for (size_t i = 0; i < 2; i++) {
        float c = a->center.c[i] + a->vel.c[i];
        float min_c = min_pos.c[i] + a->radius;
        if (c < min_c) {
            c = 2 * min_c - c;
            a->vel.c[i] = ABS(a->vel.c[i]);
        }
        float max_c = max_pos.c[i] - a->radius;
        if (c > max_c) {
            c = 2 * max_c - c;
            a->vel.c[i] = -ABS(a->vel.c[i]);
        }
        a->center.c[i] = c;
    }

    // Update opacity.
    a->opacity += a->opa_vel;
    if (a->opacity > 1.0) {
        a->opacity = 2 - a->opacity;
        a->opa_vel = -ABS(a->opa_vel);
    } else if (a->opacity < 0.0) {
        a->opacity = -a->opacity;
        a->opa_vel = ABS(a->opa_vel);
    }
    
    // Update star points.
    for (size_t i = 0; i < 5; i++) {
        float angle = a->angle + i * M_PI * 0.40;
        a->points[i].x = a->center.x + a->radius * cosf(angle);
        a->points[i].y = a->center.y + a->radius * sinf(angle);
    }

    // Update star inner points.
    for (size_t i = 0; i < 5; i++) {
        point *p00 = &a->points[i];
        point *p01 = &a->points[(i + 2) % 5];
        point *p10 = &a->points[(i + 1) % 5];
        point *p11 = &a->points[(i + 3) % 5];
        line_intersection(p00->x, p00->y, p01->x, p01->y,
                          p10->x, p10->y, p11->x, p11->y,
                          &a->in_pts[i].x, &a->in_pts[i].y);
    }
}

static void animate(void)
{
    if (new_mode != current_mode) {
        current_mode = new_mode;
        video_set_bg_color(pack_color(demo_modes[current_mode].bg_color), true);
        star_opacity = 0xFF;
        mode_start_msec = system_millis;
    }
    
#if 0
    // Update rotation angle.
    angle += ROTATION_RATE;
    if (angle >= 2 * M_PI)
        angle -= 2 * M_PI;

    // Update opacity.
    static int op_inc = +5;
    star_opacity += op_inc;
    if (star_opacity >= 0xFF) {
        star_opacity = 0x1FE - star_opacity;
        op_inc = -op_inc;
    } else if (star_opacity <= 0) {
        star_opacity *= -1;
        op_inc = -op_inc;
        
    }

    // Update position.
#ifdef MOVE
    static coord x_inc = INT_TO_COORD(+1);
    static coord y_inc = INT_TO_COORD(+1);
#else
    static coord x_inc = INT_TO_COORD(+0);
    static coord y_inc = INT_TO_COORD(+0);
#endif
    center[0] += x_inc;
    center[1] += y_inc;
    if (x_inc > 0 && center[0] > int_to_coord(CENTER_X_MAX)) {
        x_inc = -x_inc;
        center[0] += x_inc;
    } else if (x_inc < 0 && center[0] < int_to_coord(CENTER_X_MIN)) {
        x_inc = -x_inc;
        center[0] += x_inc;
    }
    if (y_inc > 0 && center[1] > int_to_coord(CENTER_Y_MAX)) {
        y_inc = -y_inc;
        center[1] += y_inc;
    } else if (y_inc < 0 && center[1] < int_to_coord(CENTER_Y_MIN)) {
        y_inc = -y_inc;
        center[1] += y_inc;
    }

    // Recalculate star points.

    for (size_t i = 0; i < 5; i++) {
        float a = angle + i * 360 / 5 * M_PI / 180;
        points[i][0] = coord_to_float(center[0]) + STAR_RADIUS * sinf(a);
        points[i][1] = coord_to_float(center[1]) - STAR_RADIUS * cosf(a);
    }
    for (size_t i = 0; i < 5; i++) {
        float *p00 = points[i];
        float *p01 = points[(i + 2) % 5];
        float *p10 = points[(i + 1) % 5];
        float *p11 = points[(i + 3) % 5];
        float x, y;
        line_intersection(p00[0], p00[1], p01[0], p01[1],
                          p10[0], p10[1], p11[0], p11[1],
                          &x, &y);
        in_pts[i][0] = x;
        in_pts[i][1] = y;
    }

    // Recalculate extra star points.
    // XXX write me
#else
    for (size_t i = 0; i < 6; i++)
        anim_state_update(&anim_states[i]);
    memcpy(points, anim_states[0].points, sizeof points);
    memcpy(in_pts, anim_states[0].in_pts, sizeof in_pts);
    star_opacity = 255 * anim_states[0].opacity;
#endif
}

#define INST_DELAY_MSEC 2500
#define INST_FADE_FREQ 0.625f

static inline coord inst_alpha(const mode_settings *mode)
{
    switch (mode->inst_anim) {

        case TA_ALL:
            return int_to_coord(1);

        case TA_DELAY:
            if (system_millis < mode_start_msec + INST_DELAY_MSEC)
                return int_to_coord(0);
            else
                return int_to_coord(1);

        case TA_FADE:
            {
                float t = (system_millis - mode_start_msec) * 0.001f;
                t *= 2 * INST_FADE_FREQ;
                float i = (int)t;
                float f = t - i;
                if ((int)t & 1)
                    f = 1.0f - f;
                return float_to_coord(f * f);
            }

        case TA_RAINBOW:
            return int_to_coord(1);

    default:
        assert(0);
    }
}

static inline rgb888 inst_color(const mode_settings *mode)
{
    switch (mode->inst_anim) {

        case TA_ALL:
            return mode->tx_color;

        case TA_DELAY:
            return mode->tx_color;

        case TA_FADE:
            return mode->tx_color;

        case TA_RAINBOW:
            {
                uint32_t msec = system_millis - mode_start_msec;
                uint8_t r, b, g;
                uint8_t up = msec & 0xFF;
                uint8_t dn = 0xff - up;
                switch ((msec >> 8) % 6) {

                    case 0:
                        r = 255;
                        g = up;
                        b = 0;
                        break;

                    case 1:
                        r = dn;
                        g = 255;
                        b = 0;
                        break;

                    case 2:
                        r = 0;
                        g = 255;
                        b = up;
                        break;

                    case 3:
                        r = 0;
                        g = dn;
                        b = 255;
                        break;

                    case 4:
                        r = up;
                        g = 0;
                        b = 255;
                        break;

                    case 5:
                        r = 255;
                        g = 0;
                        b = dn;
                        break;
                }
                return r << 16 | g << 8 | b << 0;
            }

    default:
        assert(0);
    }
}

static void draw_instructions(pixtile *tile, const mode_settings *mode)
{
    int y0 = INST_Y;
    int y1 = y0 + TEXT_PIXMAP_HEIGHT;
    if (y0 > (int)(tile->y + tile->height) || y1 < (int)tile->y)
        return;

    coord opacity = inst_alpha(mode);
    if (!opacity)
        return;
    rgb888 color = inst_color(mode);
    const text_pixmap *pixmap = mode->instructions;

    int x0 = (SCREEN_WIDTH - TEXT_PIXMAP_WIDTH) / 2;
    int x1 = (SCREEN_WIDTH + TEXT_PIXMAP_WIDTH) / 2;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            coord alpha = (*pixmap)[y - y0][x - x0];
            alpha = coord_product(alpha, opacity);
            blend_pixel_unclipped_co(tile, x, y, color, alpha);
        }
    }
}

static void draw_tile(pixtile *tile)
{
    const mode_settings *mode = &demo_modes[current_mode];
    (*mode->op)(tile, mode);
    draw_instructions(tile, mode);
}

static void draw_frame(void)
{
    size_t h;

    for (size_t y = 0; y < SCREEN_HEIGHT; y += h) {
        h = MIN((size_t)PIXTILE_MAX_HEIGHT, SCREEN_HEIGHT - y);
        pixtile *tile = alloc_pixtile(y, h);
        draw_tile(tile);
        send_pixtile(tile);
    }
}

uint32_t fps;

static void calc_fps(void)
{
    static uint32_t next_time;
    static uint32_t frame_count;
    frame_count++;
    if (system_millis >= next_time) {
        fps = frame_count;
        frame_count = 0;
        next_time += 1000;
    }
}

static debounce mode_button;

static void poll_button(uint32_t millis)
{
    (void)millis;
    if (debounce_update(&mode_button)) {
        if (debounce_is_falling_edge(&mode_button)) {
            new_mode = (current_mode + 1) % DRAWING_MODE_COUNT;
        }
    }
}

static void setup_button(void)
{
    static const gpio_pin mode_button_pin = {
        .gp_port   = GPIOC,
        .gp_pin    = GPIO1,
        .gp_mode   = GPIO_MODE_INPUT,
        .gp_pupd   = GPIO_PUPD_PULLUP,
    };
    const uint32_t BUTTON_SETTLE_MSEC = 20;

    init_debounce(&mode_button, &mode_button_pin, BUTTON_SETTLE_MSEC);
    register_systick_handler(poll_button);
}

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);
    flash_prefetch_enable();
    flash_icache_enable();
    flash_dcache_enable();

    setup_systick(MY_CLOCK.ahb_frequency);
    // setup_heartbeat();

    video_set_bg_color(BG_COLOR, false);
    setup_video();

    setup_button();

#if 0
    // Start star at center of screen.
    center[0] = int_to_coord(SCREEN_WIDTH / 2);
    center[1] = int_to_coord(SCREEN_HEIGHT / 2);
#else
    setup_animation();
#endif
}

volatile float xtrnangle = 2.222;
volatile float xtrnflotex;
volatile float xtrnflotey;

static void thousand_trig(void)
{
    for (int i = 0; i < 500; i++) {
        xtrnflotex = cosf(xtrnangle);
        xtrnflotey = sinf(xtrnangle);
    }
}

static void run(void)
{
    while (1) {
        animate();
        draw_frame();
        calc_fps();
    }

    if (0)
    while (1) {
        thousand_trig();
        calc_fps();
    }
}

int main(void)
{
    setup();
    run();
}
