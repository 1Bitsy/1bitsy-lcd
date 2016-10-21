//#define TWO_LINES
//#define ONE_TRI
//#define DEFAULT_DRAWING_MODE MODE_FILL

#include <assert.h>
#include <math.h>

#include <libopencm3/stm32/rcc.h>

#include "coord.h"
#include "debounce.h"
#include "systick.h"
#include "util.h"
#include "video.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])


// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

static inline void blend_pixel_unclipped(pixtile *tile,
                                         int x, int y,
                                         uint16_t color, coord alpha)
{
    uint16_t *p = &tile->pixels[y - tile->y][x];
    uint16_t pix = *p;
    uint16_t pr = pix >> 8 & 0xf8;
    uint16_t pg = pix >> 3 & 0xfc;
    uint16_t pb = pix << 3 & 0xf8;
    uint16_t cr = color >> 8 & 0xf8;
    uint16_t cg = color >> 3 & 0xfc;
    uint16_t cb = color << 3 & 0xf8;

    pr += coord_to_int((cr - pr) * alpha);
    pg += coord_to_int((cg - pg) * alpha);
    pb += coord_to_int((cb - pb) * alpha);
    if (pr > 255) pr = 255;
    if (pg > 255) pg = 255;
    if (pb > 255) pb = 255;
    *p = (pr << 8 & 0xf800) | (pg << 3 & 0x07e0) | (pb >> 3 & 0x001f);
}

static inline void blend_pixel(pixtile *tile,
                               int x, int y,
                               uint16_t color, coord alpha)
{
    if (x_in_pixtile(tile, x) && y_in_pixtile(tile, y)) {
        blend_pixel_unclipped(tile, x, y, color, alpha);
    }
}

/*static*/ void draw_line(pixtile *tile,
                      coord x0, coord y0,
                      coord x1, coord y1,
                      uint16_t color);
/*static*/ void draw_line(pixtile *tile,
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

/*static*/ void draw_line_aa(pixtile *tile,
                         coord x0, coord y0,
                         coord x1, coord y1,
                         uint16_t color,
                         uint8_t alpha);
/*static*/ void draw_line_aa(pixtile *tile,
                         coord x0, coord y0,
                         coord x1, coord y1,
                         uint16_t color,
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

/*static*/ void fill_triangle(pixtile *tile,
                          coord verts[3][2],
                          uint16_t color);
/*static*/ void fill_triangle(pixtile *tile,
                          coord verts[3][2],
                          uint16_t color)
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
#define BG_COLOR      0x0000
#define CENTER_X_MIN (STAR_RADIUS - 10)
#define CENTER_X_MAX (SCREEN_WIDTH - STAR_RADIUS + 10)
#define CENTER_Y_MIN (STAR_RADIUS - 10)
#define CENTER_Y_MAX (SCREEN_HEIGHT - STAR_RADIUS + 10)

enum drawing_mode {
    MODE_OUTLINE,
    MODE_OUTLINE_AA,
    MODE_OUTLINE_AA_FADE,
    MODE_FILL,
    MODE_FILL_AA,
    MODE_FILL_AA_FADE,
    MODE_MORE,
    DRAWING_MODE_COUNT          // must be last
} drawing_mode
#ifdef DEFAULT_DRAWING_MODE
               = DEFAULT_DRAWING_MODE
#endif
                                     ;

float angle = 0.0;
coord points[5][2];
coord in_pts[5][2];
coord center[2];
int star_opacity = 0;

static void animate(void)
{
#ifdef TWO_LINES
    static coord tran;

    tran += 3;
    if (tran > int_to_coord(100))
        tran = 0;

    points[0][0] = int_to_coord(50);
    points[0][1] = tran;
    points[1][0] = int_to_coord(190);
    points[1][1] = int_to_coord(20) + tran;
    points[2][0] = tran;
    points[2][1] = int_to_coord(50);
    points[3][0] = int_to_coord(20) + tran;
    points[3][1] = int_to_coord(190);
#else

    // // Update drawing mode.
    // static uint32_t next_time = 2000;
    // if (system_millis >= next_time) {
    //     drawing_mode = (drawing_mode + 1) % DRAWING_MODE_COUNT;
    //     drawing_mode = MODE_FILL;
    //     next_time += 2000;
    // }

    // Update rotation angle.
    angle += ROTATION_RATE;
    if (angle >= 2 * M_PI)
        angle -= 2 * M_PI;

    // Update opacity.
    static int op_inc = +1;
    star_opacity += op_inc;
    if (star_opacity >= 0xFF) {
        star_opacity = 0xFF;
        op_inc = -1;
    } else if (star_opacity <= 0) {
        star_opacity = 0;
        op_inc = +1;
        
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
#endif
}

static void outline_star(pixtile *tile)
{
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord *p0 = points[i];
        coord *p1 = points[j];
        draw_line(tile, p0[0], p0[1], p1[0], p1[1], 0xf800);
    }
}

static void outline_star_aa(pixtile *tile, uint16_t color, uint8_t alpha)
{
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord *p0 = points[i];
        coord *p1 = points[j];
        draw_line_aa(tile, p0[0], p0[1], p1[0], p1[1], color, alpha);
    }
}

static void fill_star(pixtile *tile)
{
    // fill_triangle(tile, points + 0, 0x0ffe0);
    // fill_triangle(tile, points + 1, 0x07ff);
    // fill_triangle(tile, points + 2, 0x0f87f);
    for (size_t i = 0; i < 5; i++) {
        size_t j = (i + 3) % 5;
        size_t k = (i + 4) % 5;
        coord tri_pts[3][2] = {
            { points[i][0], points[i][1] },
            { in_pts[j][0], in_pts[j][1] },
            { in_pts[k][0], in_pts[k][1] },
        };
        fill_triangle(tile, tri_pts, 0xffe0);
    }
}

static void draw_tile(pixtile *tile)
{
#ifdef TWO_LINES
    coord *p0 = points[0];
    coord *p1 = points[1];
    coord *p2 = points[2];
    coord *p3 = points[3];
    draw_line_aa(tile, p0[0], p0[1], p1[0], p1[1], 0xF800, 0x44);
    draw_line_aa(tile, p2[0], p2[1], p3[0], p3[1], 0x07E0, 0x44);
#elif defined(ONE_TRI)
    fill_triangle(tile, points, 0x07FF);
#else
    switch (drawing_mode) {

    case MODE_OUTLINE:
        outline_star(tile);
        break;

    case MODE_OUTLINE_AA:
        outline_star_aa(tile, 0x07E0, 0xFF);
        break;

    case MODE_OUTLINE_AA_FADE:
        outline_star_aa(tile, 0x001F, star_opacity);
        break;

    case MODE_FILL:
        fill_star(tile);
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
            drawing_mode = (drawing_mode + 1) % DRAWING_MODE_COUNT;
            star_opacity = 0xFF;
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
        // .gp_af     = GPIO_AF0,
        // .gp_ospeed = GPIO_OSPEED_DEFAULT,
        // .gp_otype  = GPIO_OTYPE_DEFAULT,
        // .gp_level  = 0,
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

    video_set_bg_color(BG_COLOR);
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
