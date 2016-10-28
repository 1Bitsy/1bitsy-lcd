#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define ABSF(x) (fabsf((x)))
#define FRACF(x) ((x) - floorf((x)))

typedef struct point {
    float x, y;
} point;

void plot(int x, float y)
{
    printf("%d %g\n", x, y);
}

void plot3(int x, float y, float err)
{
    printf("%d %g %+g\n", x, y, err);
}

void dda_print(float x0, float y0, float x1, float y1)
{
    assert(x0 != x1);
    float m = (y1 - y0) / (x1 - x0);
    assert(-1.0 <= m && m <= +1.0);
    float b = y0 - m * x0;
    for (int ix = round(x0); ix <= round(x1); ix++) {
        float y = m * ix + b;
        plot(ix, y);
    }
}

// r = rounded
// f = floor
// c = ceiling
// s = sub-integer
// t = 1 - s

void dda_print2(float x0, float y0, float x1, float y1)
{
    float dx = x1 - x0;
    float dy = y1 - y0;
    // printf("dy=%g dx=%g\n", dy, dx);
    assert(dx > 0);
    assert(dy <= dx);
    // float m = dy / dx;
    // float b = y0 - m * x0;

    int ix = roundf(x0);
    int iy = roundf(y0);
    int i = 0;
    while (ix <= roundf(x1)){
        float err = (iy - y0) * dx + (ix - x0) * dy;
        plot3(ix, iy, err);
        float err_x = (iy + 1 - y0) * dx - (ix - x0) * dy;
        float err_y = (iy - y0) * dx - (ix + 1 - x0) * dy;
        float err_xy = (iy + 1 - y0) * dx - (ix + 1 - x0) * dy;
#if 1
        if (err_x + err_xy > 0) {
            ix += 1;
        }
        if (err_y + err_xy < 0) {
            iy += 1;
        }
#else
        if (err_x > 0)
            ix += 1;
        if (err_y < 0)
            iy += 1;
#endif
        // assert(err_x + err_xy > 0 || err_y + err_xy < 0);
        if (++i >= 10)
            break;
    }
}

void dda_print3(float x0, float y0, float x1, float y1)
{
    float dx = ABSF(x1 - x0);
    float dy = -ABSF(y1 - y0);
    int sx = (x1 < x0) ? -1 : +1;
    int sy = (x1 < x0) ? -1 : +1;
    float m = (y1 - y0) / (x1 - x0);
    float b = y0 - m * x0;
    // printf("m = %g, b = %g\n", m, b);

    int ix = roundf(x0);
    int iy = roundf(y0);
    float err = (iy + 1 - y0) * dx + (ix + 1 - x0) * dy;
    float e2;
    int i = 0;
    while (1) {
        plot3(ix, iy, m * ix + b - iy);
        e2 = 2 * err;
        if (e2 >= dy) {
            if (ix == roundf(x1))
                break;
            err += dy;
            ix += sx;
        }
        if (e2 <= dx) {
            if (iy == roundf(y1))
                break;
            err += dx;
            iy += sy;
        }
        if (++i >= 10)
            break;
    }
}

void dda_print4(float x0, float y0, float x1, float y1)
{
    float dx = ABSF(x1 - x0);
    float dy = -ABSF(y1 - y0);
    int sx = (x1 < x0) ? -1 : +1;
    int sy = (x1 < x0) ? -1 : +1;
    float m = (y1 - y0) / (x1 - x0);
    float b = y0 - m * x0;
    printf("m = %g, b = %g\n", m, b);

    int ix = floorf(x0);
    int iy = floorf(y0);
    float err = (iy + 1 - y0) * dx + (ix + 1 - x0) * dy;
    float e2;
    float ee = m * (ix + 0.5) + b - iy;
    while (1) {
        plot3(ix, iy, ee);
        e2 = 2 * err;
        if (e2 >= dy) {
            if (ix == floorf(x1))
                break;
            err += dy;
            ee += m;
            ix += sx;
        }
        if (e2 <= dx) {
            if (iy == floorf(y1))
                break;
            err += dx;
            ee -= 1;
            iy += sy;
        }
    }
}

void dda_print5(float x0, float y0, float x1, float y1)
{
    float dx = ABSF(x1 - x0);
    float dy = -ABSF(y1 - y0);
    int sx = (x1 < x0) ? -1 : +1;
    int sy = (y1 < y0) ? -1 : +1;
    float m = (y1 - y0) / (x1 - x0);
    bool steep = ABSF(m) > 1;
    float inv_m = (x1 - x0) / (y1 - y0);
    float b = y0 - m * x0;
    float magic_k = inv_m / 2;

    int ix = floor(x0);
    int iy = floor(y0);
    // float cover = 1 + iy - m * (ix + 0.5) - b;
    float cover = 0;
    float ncover = 0;
    while (1) {
        if (ix >= x1)
            break;
        plot3(ix, iy, cover);
        if (ncover)
            printf("    ncover = %g\n", ncover);
        float y = m * ix + b;
        float ny = m * (ix + 1) + b;
        if (ny >= iy + 1) {
            // step up
            float nys = ny - iy;
            cover = ncover + 1 - magic_k * nys * nys;
            ncover = -magic_k * (iy + 1 - y);
            printf("step up: cover = 1 - %g * (%g - %g) = %g\n",
                   magic_k, ny, floorf(ny), cover);
            iy++;
        } else {
            // step over
            ix++;
            float yh = m * (ix + 0.5) + b;
            cover = ncover + ceilf(yh) - yh;
            ncover = 0;
        }
    }
}

void dda_print6(float x0, float y0, float x1, float y1)
{
    float m = (y1 - y0) / (x1 - x0);
    float b = y0 - m * x0;
    int ix = floorf(x0);
    int iy = floorf(y0);
    float tri = 0.5 / m;
    float y, ny;

    while (ix < x1) {
        bool s2 = true;
        while (1) {
            y = m * ix + b;
            ny = m * (ix + 1) + b;
            if (ny <= iy + 1)
                break;
            float cover;
            if (s2)
                cover = tri * (y - iy - 1) * (y - iy - 1) + 10;
            else
                cover = FRACF(iy + 0.5 - b) / m + 20;
            plot3(ix, iy, cover);
            iy++;
            s2 = false;
        }
        float cover;
        if (y < iy)
            cover = 1 - tri * (ny - iy) * (ny - iy) + 30;
        else
            cover = FRACF(m * (ix + 0.5) + b) + 40;
        plot3(ix, iy, cover);
        // plot(ix, iy + 1);
        ix++;
    }
}

int main()
{
    point p0 = { 1, 1.5 };
    point p1 = { 4, 3.5 };
    point p2 = { 1, 1.5 };
    point p3 = { 3, 4.5 };
    point p4 = { 1, 3.5 };
    point p5 = { 4, 3.5 };

    putchar('\n');
    printf("(%g, %g) <-> (%g, %g)\n\n", p0.x, p0.y, p1.x, p1.y);

    // dda_print(p0.x, p0.y, p1.x, p1.y);
    // putchar('\n');
    // dda_print2(p0.x, p0.y, p1.x, p1.y);
    // putchar('\n');
    // dda_print3(p0.x, p0.y, p1.x, p1.y);
    // putchar('\n');
    // dda_print4(p0.x, p0.y, p1.x, p1.y);
    // putchar('\n');
    // dda_print5(p0.x, p0.y, p1.x, p1.y);
    // putchar('\n');
    dda_print6(p0.x, p0.y, p1.x, p1.y);
    putchar('\n');
    dda_print6(p2.x, p2.y, p3.x, p3.y);
    putchar('\n');
    dda_print6(p4.x, p4.y, p5.x, p5.y);
    return 0;
}
