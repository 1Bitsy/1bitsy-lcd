#include <assert.h>
#include <math.h>

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
// Modifiers: aa, blended

// Colors: rgb888, rgb555, gray8

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

typedef uint16_t rgb565;
typedef uint32_t rgb888;        // 0x00rrggbb

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

static inline void blend_pixel_unclipped(pixtile *tile,
                                         int x, int y,
                                         rgb888 color, coord alpha)
{
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

static inline void blend_pixel(pixtile *tile,
                               int x, int y,
                               rgb888 color, coord alpha)
{
    if (x_in_pixtile(tile, x) && y_in_pixtile(tile, y)) {
        blend_pixel_unclipped(tile, x, y, color, alpha);
    }
}

static void draw_line(pixtile *tile,
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

static void draw_line_aa(pixtile *tile,
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
        blend_pixel(tile, ypxl1, xpxl1, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel(tile, ypxl1 + 1, xpxl1, color, a);
    } else {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel(tile, xpxl1, ypxl1, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel(tile, xpxl1, ypxl1 + 1, color, a);
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
        blend_pixel(tile, ypxl2, xpxl2, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel(tile, ypxl2 + 1, xpxl2, color, a);
    } else {
        a = coord_to_int(coord_product(coord_rfrac(yend), xgap));
        blend_pixel(tile, xpxl2, ypxl2, color, a);
        a = coord_to_int(coord_product(coord_frac(yend), xgap));
        blend_pixel(tile, xpxl2, ypxl2 + 1, color, a);
    }

    // main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_rfrac(float_to_coord(intery))));
            blend_pixel(tile, (int)intery, x, color, a);
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_frac(float_to_coord(intery))));
            blend_pixel(tile, (int)intery + 1, x, color, a);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_rfrac(float_to_coord(intery))));
            blend_pixel(tile, x, (int)intery, color, a);
            a = coord_to_int(
                coord_product(int_to_coord(alpha),
                              coord_frac(float_to_coord(intery))));
            blend_pixel(tile, x, (int)intery + 1, color, a);
            intery += gradient;
        }
    }
}

// project x onto the line (x0,y0) <-> (x1,y1) and return y.
static inline coord
project_x_to_line(coord x, coord x0, coord y0, coord x1, coord y1)
{
    if (x1 == x0)
        return y0;
    return y0 + coord_quotient(coord_product(x - x0, y1 - y0), x1 - x0);
}

// project y onto the line (x0,y0) <-> (x1,y1) and return x.
static inline coord
project_y_to_line(coord y, coord x0, coord y0, coord x1, coord y1)
{
    if (y1 == y0)
        return x0;
    return x0 + coord_quotient(coord_product(y - y0, x1 - x0), y1 - y0);
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

static void fill_trapezoid(pixtile *tile,
                           coord xl0, coord xr0, coord y0,
                           coord xl1, coord xr1, coord y1,
                           uint16_t color)
{
    assert(y0 <= y1);
    assert(xl0 <= xr0);
    assert(xl1 <= xr1);

    // Clip top and bottom.
    coord min_y = int_to_coord(tile->y);
    coord max_y = int_to_coord(tile->y + tile->height);
    if (y0 < min_y) {
        xl0 = project_y_to_line(min_y, xl0, y0, xl1, y1);
        xr0 = project_y_to_line(min_y, xr0, y0, xr1, y1);
        y0 = min_y;
    }
    if (y1 >= max_y) {
        xl1 = project_y_to_line(max_y, xl0, y0, xl1, y1);
        xr1 = project_y_to_line(max_y, xr0, y0, xr1, y1);
        y1 = max_y;
    }

    coord dy = y1 - y0;
    coord dxl = xl1 - xl0;
    coord dxr = xr1 - xr0;
    int iy0 = coord_to_int(y0);
    int iy1 = coord_to_int(y1);
    for (int iy = iy0; iy < iy1; iy++) {
        int xl = xl0 + coord_quotient((iy - iy0) * dxl, dy);
        int xr = xr0 + coord_quotient((iy - iy0) * dxr, dy);
        int ixl = MAX(0, coord_to_int(xl));
        int ixr = MIN(PIXTILE_WIDTH,coord_to_int(xr));
        for (int ix = ixl; ix < ixr; ix++)
            tile->pixels[iy - tile->y][ix] = color;
    }
}

static void fill_triangle(pixtile *tile, coord verts[3][2], uint16_t color)
{
    coord x0, y0, x1, y1, x2, y2;

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

    coord px1 = project_y_to_line(y1, x0, y0, x2, y2);

    coord xl1 = (px1 < x1) ? px1 : x1;
    coord xr1 = (px1 < x1) ? x1 : px1;
    fill_trapezoid(tile, x0, x0, y0, xl1, xr1, y1, color);
    fill_trapezoid(tile, xl1, xr1, y1, x2, x2, y2, color);
}                          


// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

#define ROTATION_RATE 0.005      // radians/frame
#define STAR_RADIUS   (0.44 * MIN(SCREEN_HEIGHT, SCREEN_WIDTH))
#define FG_COLOR      0xffff
#define BG_COLOR      0x0802
#define CENTER_X_MIN (STAR_RADIUS - 20)
#define CENTER_X_MAX (SCREEN_WIDTH - STAR_RADIUS + 20)
#define CENTER_Y_MIN (STAR_RADIUS - 20)
#define CENTER_Y_MAX (SCREEN_HEIGHT - STAR_RADIUS + 20)
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

static float angle = 0.0;
static coord points[5][2];
static coord in_pts[5][2];
static coord center[2];
static int star_opacity = 0;


static void outline_star(pixtile *tile, rgb888 color)
{
    rgb565 pcolor = pack_color(color);
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord *p0 = points[i];
        coord *p1 = points[j];
        draw_line(tile, p0[0], p0[1], p1[0], p1[1], pcolor);
    }
}

static void outline_star_aa(pixtile *tile, rgb888 color, uint8_t alpha)
{
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord *p0 = points[i];
        coord *p1 = points[j];
        draw_line_aa(tile, p0[0], p0[1], p1[0], p1[1], color, alpha);
    }
}

static void fill_star(pixtile *tile, rgb888 color)
{
    rgb565 pcolor = pack_color(color);
    for (size_t i = 0; i < 5; i++) {
        size_t j = (i + 3) % 5;
        size_t k = (i + 4) % 5;
        coord tri_pts[3][2] = {
            { points[i][0], points[i][1] },
            { in_pts[j][0], in_pts[j][1] },
            { in_pts[k][0], in_pts[k][1] },
        };
        fill_triangle(tile, tri_pts, pcolor);
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
    int opacity = star_opacity * star_opacity / 255;
    outline_star_aa(tile, mode->fg_color, opacity);
}

static void draw_filled(pixtile *tile, const mode_settings *mode)
{
    fill_star(tile, mode->fg_color);
}

static void draw_filled_aa(pixtile *tile, const mode_settings *mode)
{
    (void)tile;
    (void)mode;
    // XXX write me
}

static void draw_filled_fa(pixtile *tile, const mode_settings *mode)
{
    (void)tile;
    (void)mode;
    // XXX write me
}

static void draw_everything(pixtile *tile, const mode_settings *mode)
{
    (void)tile;
    (void)mode;
    // XXX write me
}

static const mode_settings demo_modes[] = {
//    bg_color  fg_color  tx_color  op               inst      inst_anim
    { 0x000000, 0xff0000, 0xffff00, draw_outline,    &pb2aa,   TA_DELAY   },
    { 0x000018, 0x00ff00, 0xff00ff, draw_outline_aa, &pb2fade, TA_DELAY   },
    { 0x202020, 0x0000ff, 0x00ffff, draw_outline_fa, &pb2fill, TA_FADE    },
    { 0x080018, 0xffff00, 0x00ff00, draw_filled,     &pb2aa,   TA_FADE    },
    { 0x002000, 0xff00ff, 0xffff00, draw_filled_aa,  &pb2fade, TA_FADE    },
    { 0x000000, 0xffff00, 0x0000ff, draw_filled_fa,  &pb4more, TA_FADE    },
    { 0xffffff, 0x110000, 0x223322, draw_everything, &pb4less, TA_RAINBOW },
};
static const size_t mode_count = (&demo_modes)[1] - demo_modes;

static void animate(void)
{
    // // Update drawing mode.
    // static uint32_t next_time = 2000;
    // if (system_millis >= next_time) {
    //     new_mode = (drawing_mode + 1) % DRAWING_MODE_COUNT;
    //     next_time += 2000;
    // }

    if (new_mode != current_mode) {
        current_mode = new_mode;
        video_set_bg_color(pack_color(demo_modes[current_mode].bg_color), true);
        star_opacity = 0xFF;
        mode_start_msec = system_millis;
    }
    
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
    static coord x_inc = INT_TO_COORD(+1);
    static coord y_inc = INT_TO_COORD(+1);
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
    float fpoints[5][2];

    for (size_t i = 0; i < 5; i++) {
        float a = angle + i * 360 / 5 * M_PI / 180;
        fpoints[i][0] = coord_to_float(center[0]) + STAR_RADIUS * sinf(a);
        fpoints[i][1] = coord_to_float(center[1]) - STAR_RADIUS * cosf(a);
        points[i][0] = float_to_coord(fpoints[i][0]);
        points[i][1] = float_to_coord(fpoints[i][1]);
    }
    for (size_t i = 0; i < 5; i++) {
        float *p00 = fpoints[i];
        float *p01 = fpoints[(i + 2) % 5];
        float *p10 = fpoints[(i + 1) % 5];
        float *p11 = fpoints[(i + 3) % 5];
        float x, y;
        line_intersection(p00[0], p00[1], p01[0], p01[1],
                          p10[0], p10[1], p11[0], p11[1],
                          &x, &y);
        in_pts[i][0] = float_to_coord(x);
        in_pts[i][1] = float_to_coord(y);
    }

    // Recalculate extra star points.
    // XXX write me
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
    rgb888 color = inst_color(mode);
    const text_pixmap *pixmap = mode->instructions;

    int x0 = (SCREEN_WIDTH - TEXT_PIXMAP_WIDTH) / 2;
    int x1 = (SCREEN_WIDTH + TEXT_PIXMAP_WIDTH) / 2;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            coord alpha = (*pixmap)[y - y0][x - x0];
            alpha = coord_product(alpha, opacity);
            blend_pixel_unclipped(tile, x, y, color, alpha);
        }
    }
}

static void draw_tile(pixtile *tile)
{
#if 0
    draw_instructions(tile, &pb2aa, 0x0000FF);
    switch (drawing_mode) {

    case MODE_OUTLINE:
        outline_star(tile, 0xFF0000);
        break;

    case MODE_OUTLINE_AA:
        outline_star_aa(tile, 0x00ff00, 0xFF);
        break;

    case MODE_OUTLINE_AA_FADE:
        outline_star_aa(tile, 0x0000ff, star_opacity);
        break;

    case MODE_FILL:
        fill_star(tile, 0xFFFF00);
        break;

    case MODE_FILL_AA:
        break;

    case MODE_FILL_AA_FADE:
        break;

    case MODE_MORE:
        break;

    default:
        break;
    }
#else
    const mode_settings *mode = &demo_modes[current_mode];
    // draw_instructions(tile, mode->instructions, mode->tx_color);
    draw_instructions(tile, mode);
    (*mode->op)(tile, mode);
#endif
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

    setup_systick(MY_CLOCK.ahb_frequency);
    // setup_heartbeat();

    video_set_bg_color(BG_COLOR, false);
    setup_video();

    setup_button();

    // Start star at center of screen.
    center[0] = int_to_coord(SCREEN_WIDTH / 2);
    center[1] = int_to_coord(SCREEN_HEIGHT / 2);
}

static void run(void)
{
    while (1) {
        animate();
        draw_frame();
        calc_fps();
    }
}

int main(void)
{
    setup();
    run();
}
