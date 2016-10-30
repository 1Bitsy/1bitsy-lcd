#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#define ABSF(x) (fabsf((x)))
#define FRACF(x) ((x) - floorf((x)))
#define FEQ(x, y) ((x) - 0.0002 <= (y) && (y) <= (x) + 0.0002)

typedef struct point {
    float x, y;
} point;

typedef struct expectation {
    int x;
    float y;
    float err;
} expectation;

const expectation expected[] = {
    { 1, 1, +10.1875 },
    { 1, 2, +30.9792 },
    { 2, 2, +40.5    },
    { 3, 2, +10.0208 },
    { 3, 3, +30.8125 },

    { 1, 1, +10.0833 },
    { 1, 2, +30.6667 },
    { 2, 2, +10      },
    { 2, 3, +20.3333 },
    { 2, 4, +30.9167 },

    { 1, 3, +50.8125 },
    { 1, 2, +70.0208 },
    { 2, 2, +80.5    },
    { 3, 2, +50.9792 },
    { 3, 1, +70.1875 },

    { 1, 4, +50.9167 },
    { 1, 3, +70.3333 },
    { 2, 3, +51      },
    { 2, 2, +60.6667 },
    { 2, 1, +70.0833 },
};

size_t ex_count = (&expected)[1] - expected;

void plot(int x, float y)
{
    printf("%d %g\n", x, y);
}

void plot3(int x, float y, float err)
{
    static size_t exi = 0;
    if (exi < ex_count) {
        const expectation *p = &expected[exi];
        if (x != p->x || !FEQ(y, p->y) || !FEQ(err, p->err))
            fprintf(stderr,
                    "Expected (%d, %g, %g); got (%d, %g, %g).\n",
                    p->x, p->y, p->err, x, y, err);
        exi++;
    }
    printf("%d %g %+g\n", x, y, err);
}

void dda_print6(float x0, float y0, float x1, float y1)
{
    float m = (y1 - y0) / (x1 - x0);
    float b = y0 - m * x0;
    float tri = 0.5 / m;

    int ix = floorf(x0);
    int iy = floorf(y0);
    float y = m * ix + b;
    float ny;
    while (ix < x1) {
        bool s2 = true;
        ny = m * (ix + 1) + b;
        if (m >= 0) {
            while (ny > iy + 1) {
                float cover;
                if (s2)
                    cover = tri * (y - iy - 1) * (y - iy - 1) + 10;
                else
                    cover = FRACF((iy + 0.5 - b) / m) + 20;
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
        } else {
            while (ny < iy) {
                float cover;
                if (s2)
                    // N.B., tri < 0
                    cover = 1 + tri * (y - iy) * (y - iy) + 50;
                else
                    cover = 1 - FRACF((iy + 0.5 - b) / m) + 60;
                plot3(ix, iy, cover);
                iy--;
                s2 = false;
            }
            float cover;
            if (y > iy + 1)
                cover = -tri * (iy + 1 - ny) * (iy + 1 - ny) + 70;
            else
                cover = 1 - FRACF(m * (ix + 0.5) + b) + 80;
            plot3(ix, iy, cover);
        }
        ix++;
        y = ny;
    }
}

int main()
{
    point p0 = { 1, 1.5 };
    point p1 = { 4, 3.5 };
    point p2 = { 1, 1.5 };
    point p3 = { 3, 4.5 };
    point p4 = { 1, 3.5 };
    point p5 = { 4, 1.5 };
    point p6 = { 1, 4.5 };
    point p7 = { 3, 1.5 };

    putchar('\n');
    printf("(%g, %g) <-> (%g, %g)\n\n", p0.x, p0.y, p1.x, p1.y);

    dda_print6(p0.x, p0.y, p1.x, p1.y);
    putchar('\n');
    dda_print6(p2.x, p2.y, p3.x, p3.y);
    putchar('\n');
    dda_print6(p4.x, p4.y, p5.x, p5.y);
    putchar('\n');
    dda_print6(p6.x, p6.y, p7.x, p7.y);
    return 0;
}
