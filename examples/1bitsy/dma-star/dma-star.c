//#define NOT_STAR

#include <assert.h>
#include <math.h>

#include <libopencm3/stm32/rcc.h>

#include "systick.h"
#include "util.h"
#include "video.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define ROTATION_RATE 0.005      // radians/frame
#define STAR_RADIUS   (0.44 * MIN(SCREEN_HEIGHT, SCREEN_WIDTH))
#define FG_COLOR      0xffff
#define BG_COLOR      0x0000
#define CENTER_X_MIN STAR_RADIUS
#define CENTER_X_MAX (SCREEN_WIDTH - STAR_RADIUS)
#define CENTER_Y_MIN STAR_RADIUS
#define CENTER_Y_MAX (SCREEN_HEIGHT - STAR_RADIUS)


// --  Coordinates  -  --  --  --  --  --  --  --  --  --  --  --  --  -
// A coordinate is a signed 24.8 fixed-point integer.

typedef int32_t coord;
const coord COORD_MIN = (coord)INT32_MIN;
const coord COORD_MAX = (coord)INT32_MAX;

static inline coord float_to_coord(float x)
{
    return (coord)(x * 256.0f);
}

static inline float coord_to_float(coord x)
{
    return (float)x / 256.0f;
}

#define INT_TO_COORD(x) ((coord)((x) << 8))
static inline coord int_to_coord(int x)
{
    return INT_TO_COORD(x);
}

static inline int coord_to_int(coord x)
{
    return x / 256;
}

static inline coord coord_product(coord a, coord b)
{
    int ip = coord_to_int(a) * coord_to_int(b);
    assert(coord_to_int(COORD_MIN) <= ip && ip <= coord_to_int(COORD_MAX));
    return a * b / 256;
}

static inline coord coord_quotient(coord a, coord b)
{
    assert(b != 0);
    return 256 * a / b;
}

static inline coord coord_floor(coord x)
{
    // floor(x) + frac(x) == x
    return x & 0xFFFFFF00;
}

static inline coord coord_frac(coord x)
{
    // floor(x) + frac(x) == x
    return x & 0xFF;
}

// --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  --  -

float angle = 0.0;
coord points[5][2];
coord center[2];

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);
    // setup_heartbeat();

    video_set_bg_color(BG_COLOR);
    setup_video();

    // Start star at center of screen.
    center[0] = int_to_coord(SCREEN_WIDTH / 2);
    center[1] = int_to_coord(SCREEN_HEIGHT / 2);
}

static void animate(void)
{
#ifdef NOT_STAR
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
    // Update rotation angle.
    angle += ROTATION_RATE;
    if (angle >= 2 * M_PI)
        angle -= 2 * M_PI;

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
    for (size_t i = 0; i < 5; i++) {
        float a = angle + i * 360 / 5 * M_PI / 180;
        points[i][0] = center[0] + float_to_coord(STAR_RADIUS * sinf(a));
        points[i][1] = center[1] - float_to_coord(STAR_RADIUS * cosf(a));
    }
#endif
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

static void draw_tile(pixtile *tile)
{
#ifdef NOT_STAR
    coord *p0 = points[0];
    coord *p1 = points[1];
    coord *p2 = points[2];
    coord *p3 = points[3];
    draw_line(tile, p0[0], p0[1], p1[0], p1[1]);
    draw_line(tile, p2[0], p2[1], p3[0], p3[1]);
#else
    for (int i = 0; i < 5; i++) {
        int j = (i + 2) % 5;
        coord *p0 = points[i];
        coord *p1 = points[j];
        draw_line(tile, p0[0], p0[1], p1[0], p1[1], FG_COLOR);
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
