#include <assert.h>
#include <stdbool.h>
#include <string.h>

// clip at bottom.
// clip at top.

typedef struct point {
    float x;
    float y;
} point;

typedef struct rect {
    point min;
    point max;
} rect;

typedef struct trapezoid {
    float xl0, xr0, y0;
    float xl1, xr1, y1;
} trapezoid;

float intersect_trapezoid_left_with_x(const trapezoid *z, float x)
{
    assert(z->xl0 != z->xl1);
    float m = (z->y1 - z->y0) / (z->xl1 - z->xl0);
    return z->y0 + m * (x - z->xl0);
}

float intersect_trapezoid_right_with_x(const trapezoid *z, float x)
{
    assert(z->xr0 != z->xr1);
    float m = (z->y1 - z->y0) / (z->xr1 - z->xr0);
    return z->y0 + m * (x - z->xr0);
}

float intersect_trapezoid_left_with_y(const trapezoid *z, float y)
{
    assert(z->y0 != z->y1);
    float m_inv = (z->xl1 - z->xl0) / (z->y1 - z->y0);
    return z->xl0 + m_inv * (y - z->y0);
}

float intersect_trapezoid_right_with_y(const trapezoid *z, float y)
{
    assert(z->y0 != z->y1);
    float m_inv = (z->xr1 - z->xr0) / (z->y1 - z->y0);
    return z->xr0 + m_inv * (y - z->y0);
}

size_t clip_trapezoids_min_y(trapezoid *zoids, size_t n, float min_y)
{
    // verify trapezoids are in order.
    for (size_t i = 1; i < n; i++)
        assert(zoids[i-1].y1 == zoids[i].y0);

    size_t i;
    for (i = 0; i < n; i++)
        if (zoids[i].y1 > min_y)
            break;

    // move the good ones up.
    if (i > 0 && i < n) {
        memmove(zoids, zoids + i, (n - i) * sizeof *zoids);
        n -= i;
    }

    // adjust the top edge
    if (i < n) {
        trapezoid *z = &zoids[0];
        if (z->y0 < min_y) {
            z->xl0 = intersect_trapezoid_left_with_y(z, min_y);
            z->xr0 = intersect_trapezoid_right_with_y(z, min_y);
            z->y0  = min_y;
        }
    }

    return n;
}

size_t clip_trapezoids_max_y(trapezoid *zoids, size_t n, float max_y)
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
        if (z->y0 < max_y) {
            z->xl1 = intersect_trapezoid_left_with_y(z, max_y);
            z->xr1 = intersect_trapezoid_right_with_y(z, max_y);
            z->y1  = max_y;
        }
    }

    return n;
}

size_t clip_trapezoids_min_x(trapezoid *zoids, size_t n, float min_x)
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
            nz->xl1 = min_x;
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

size_t clip_trapezoids_max_x(trapezoid *zoids, size_t n, float max_x)
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

size_t clip_trapezoids(trapezoid z[5], size_t n, rect *bbox)
{
    n = clip_trapezoids_max_y(z, n, bbox->max.y);
    n = clip_trapezoids_min_y(z, n, bbox->min.y);
    n = clip_trapezoids_min_x(z, n, bbox->min.x);
    n = clip_trapezoids_max_x(z, n, bbox->max.x);
    return n;
}
