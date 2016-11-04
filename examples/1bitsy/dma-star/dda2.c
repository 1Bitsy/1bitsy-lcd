#define EXPECT

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define CAT2_H(a, b) a##b
#define CAT_H(a, b) CAT2_H(a, b)
#define TMPVAR_H() CAT_H(tmp__, __COUNTER__)


#define MIN(a, b)           MIN_H(a, b, TMPVAR_H(), TMPVAR_H())
#define MIN_H(a, b, t1, t2) ({                                          \
                                __typeof__ (a) t1 = (a);                \
                                __typeof__ (b) t2 = (b);                \
                                t1 < t2 ? t1 : t2;                      \
                            })

#define MAX(a, b)           MAX_H(a, b, TMPVAR_H(), TMPVAR_H())
#define MAX_H(a, b, t1, t2) ({                                          \
                                __typeof__ (a) t1 = (a);                \
                                __typeof__ (b) t2 = (b);                \
                                t1 > t2 ? t1 : t2;                      \
                            })

#define ABS(x) (fabsf((x)))
#define FRAC(x) ((x) - floorf((x)))
#define CEIL(x) ((int)ceilf(x))
#define FEQ(x, y) ((x) - 0.0002 <= (y) && (y) <= (x) + 0.0002)

typedef struct point {
    float x, y;
} point;

#ifdef EXPECT
typedef struct expectation {
    char op;
    int  x;
    int  y;
    int  alpha;
} expectation;

const expectation expected[] = {
    { 'p', 1, 1, 0x56 },
    { 'f', 2, 1, 0x80 },
    { 'p', 2, 2, 0x7f },
    { 'f', 3, 2, 0xff },
    { 'p', 3, 3, 0x00 },
    { 'p', 4, 3, 0xd5 },
    { 'f', 5, 3, 0x7f },

    { 'p', 1, 1, 0x80 },
    { 'f', 2, 1, 0x80 },
    { 'p', 1, 2, 0x55 },
    { 'f', 2, 2, 0xff },
    { 'p', 2, 3, 0xaa },
    { 'f', 3, 3, 0xff },
    { 'p', 2, 4, 0    },
    { 'f', 3, 4, 0x7f },

    { 'p', 4, 1, 0x56 },
    { 'p', 3, 1, 0x2a },
    { 'f', 5, 1, 0x80 },
    { 'p', 2, 2, 0x80 },
    { 'f', 3, 2, 0xff },
    { 'p', 1, 3, 0x55 },
    { 'f', 2, 3, 0x7f },

    { 'p', 3, 1, 0x80 },
    { 'f', 4, 1, 0x80 },
    { 'p', 2, 2, 0xaa },
    { 'f', 3, 2, 0xff },
    { 'p', 1, 3, 0x55 },
    { 'f', 2, 3, 0xff },
    { 'p', 1, 4, 0x7f },
    { 'f', 2, 4, 0x7f },

    { 'p', 8, 1, 0xff },
    { 'p', 7, 1, 0xbb },
    { 'p', 6, 1, 0x77 },
    { 'p', 5, 1, 0x33 },
    { 'f', 9, 1, 0xff },
    { 'p', 4, 2, 0xee },
    { 'p', 3, 2, 0xaa },
    { 'p', 2, 2, 0x66 },
    { 'f', 5, 2, 0xff },
    { 'p', 1, 3, 0    },
    { 'f', 2, 3, 0    },
};

size_t ex_count = (&expected)[1] - expected;
static size_t ex_index;
bool success = true;
#endif /* EXPECT */

static void pixel(int x, int y, int alpha)
{
    printf("pixel (%d %d) %#x\n", x, y, alpha);
#ifdef EXPECT
    if (ex_index < ex_count) {
        const expectation *p = &expected[ex_index++];
        if (p->op != 'p' || x != p->x || y != p->y || alpha != p->alpha) {
            fprintf(stderr,
                    "***** Expected ('%c', %d, %d, %#x); "
                    "got ('p', %d, %d, %#x).\n\n",
                    p->op, p->x, p->y, p->alpha, x, y, alpha);
            success = false;
        }
    }
#endif
}

static void fill(int x, int y, int alpha)
{
    printf("fill  (%d %d) %#x\n", x, y, alpha);
#ifdef EXPECT
    if (ex_index < ex_count) {
        const expectation *p = &expected[ex_index++];
        if (p->op != 'f' || x != p->x || y != p->y || alpha != p->alpha) {
            fprintf(stderr,
                    "**** Expected ('%c', %d, %d, %#x); "
                    "got ('f', %d, %d, %#x).\n\n",
                    p->op, p->x, p->y, p->alpha, x, y, alpha);
            success = false;
        }
    }
#endif
}

static void dda_print(float x0, float y0, float x1, float y1)
{
    printf("\n(%g, %g) <-> (%g, %g)\n\n", x0, y0, x1, y1);

    const float dx = x1 - x0;
    const float dy = y1 - y0;
    assert(dy >= 0);
    int   inc = (dx < 0) ? -1 : +1;
    float half_inc = (dx < 0) ? -0.5 : +0.5;
    int   comp = (dx < 0) ? ~0 : 0;

    const bool steep = ABS(dx) > dy;
    int interp, gradient, alpha0;
    if (steep) {
        // X axis changes faster
        float m  = dy / dx;
        interp   = (int)(65536.0 * (y0 + m * ((int)x0 - x0 + 0.5)));
        gradient = (int)(65536.0 * ABS(m));
        alpha0   = (interp ^ comp) >> 8 & 0xFF;
        // printf("steep: m = %g, interp = %#x, gradient = %#x, alpha0 = %#x\n",
        //        m, interp, gradient, alpha0);
    } else {
        // Y axis changes faster.
        const float m = dx / dy;
        interp   = (int)(65536.0 * (x0 + m * ((int)y0 + 0.5 - y0)));
        gradient = (int)(65536.0 * dx / dy);
        alpha0   = 0xFF - (interp >> 8 & 0xFF);
        // printf("flat: m = %g, interp = %#x, gradient = %#x, alpha0 = %#x\n",
        //        m, interp, gradient, alpha0);
    }
    int unfill_alpha = 0xFF * FRAC(y0);
    int fill_alpha   = 0xFF - unfill_alpha;
    int alpha        = MAX(0, alpha0 - unfill_alpha);

    const int ix1 = (int)x1;
    const int iy1 = (int)y1;
    int       ix  = (int)x0;
    int       iy  = (int)y0;
    int       pix, fix;  // X coordinates of pixel, fill.
    int       pa;        // pixel alpha 0..255

    while (iy <= iy1) {
        if (steep) {
            int iyt = iy;
            fix = ix + 1;
            int fix2 = fix;
            while (true) {
                pix = ix;
                fix2 = ix + 1;
                pa = alpha;
                interp += gradient;
                alpha = (interp ^ comp) >> 8 & 0xFF;
                iyt = interp >> 16;
                ix += inc;
                if (iyt != iy || inc * (ix - ix1) > comp)
                    break;
                pixel(pix, iyt, pa);
            }
            if (inc == +1)
                fix = fix2;
        } else {
            pix = ix;
            fix = ix + 1;
            pa = alpha;
            interp += gradient;
            alpha = 0xFF - (interp >> 8 & 0xFF);
            ix = interp >> 16;
        }
        pixel(pix, iy, pa);
        fill(fix, iy, fill_alpha);
        if (++iy == iy1) {
            fill_alpha = 0xFF * FRAC(y1);
            unfill_alpha = 0xFF - fill_alpha;
            alpha = MAX(0, alpha - unfill_alpha);
        } else {
            fill_alpha = 0xFF;
        }
    }
}

int main(void)
{
    point p0 = { 1, 1.5 };
    point p1 = { 4, 3.5 };

    point p2 = { 1, 1.5 };
    point p3 = { 3, 4.5 };

    point p4 = { 4, 1.5 };
    point p5 = { 1, 3.5 };

    point p6 = { 3, 1.5 };
    point p7 = { 1, 4.5 };

    point p8 = { 8.5, 1.0 };
    point p9 = { 1.0, 3.0 };

    dda_print(p0.x, p0.y, p1.x, p1.y);
    putchar('\n');
    dda_print(p2.x, p2.y, p3.x, p3.y);
    putchar('\n');
    dda_print(p4.x, p4.y, p5.x, p5.y);
    putchar('\n');
    dda_print(p6.x, p6.y, p7.x, p7.y);
    putchar('\n');
    dda_print(p8.x, p8.y, p9.x, p9.y);
#ifdef EXPECT
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    return 0;
#endif
}
