#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct point {
    float x;
    float y;
} point;

typedef point tri[3];

typedef struct rect {
    point min;
    point max;
} rect;

void sort_tri_by_x(tri t)
{
    float x0, y0, x1, y1, x2, y2;

    if (t[0].x <= t[1].x) {
        if (t[0].x <= t[2].x) {
            if (t[1].x <= t[2].x) {
                // v0 <= v1 <= v2
                x0 = t[0].x; y0 = t[0].y;
                x1 = t[1].x; y1 = t[1].y;
                x2 = t[2].x; y2 = t[2].y;
            } else {
                // v0 <= v2 < v1
                x0 = t[0].x; y0 = t[0].y;
                x1 = t[2].x; y1 = t[2].y;
                x2 = t[1].x; y2 = t[1].y;
            }
        } else {
            // v2 < v0 < v1
            x0 = t[2].x; y0 = t[2].y;
            x1 = t[0].x; y1 = t[0].y;
            x2 = t[1].x; y2 = t[1].y;
        }
    } else {
        // v1 < v0
        if (t[2].x <= t[1].x) {
            // v2 <= v1 < v0
            x0 = t[2].x; y0 = t[2].y;
            x1 = t[1].x; y1 = t[1].y;
            x2 = t[0].x; y2 = t[0].y;
        } else {
            // v1 < v0, v1 < v2
            if (t[0].x <= t[2].x) {
                // v1 < v0 <= v2
                x0 = t[1].x; y0 = t[1].y;
                x1 = t[0].x; y1 = t[0].y;
                x2 = t[2].x; y2 = t[2].y;
            } else {
                // v1 < v2 < v0
                x0 = t[1].x; y0 = t[1].y;
                x1 = t[2].x; y1 = t[2].y;
                x2 = t[0].x; y2 = t[0].y;
            }
        }
    }
    t[0].x = x0; t[0].y = y0;
    t[1].x = x0; t[1].y = y1;
    t[2].x = x0; t[2].y = y2;
}

void sort_tri_by_y(tri t)
{
    float x0, y0, x1, y1, x2, y2;

    if (t[0].y <= t[1].y) {
        if (t[0].y <= t[2].y) {
            if (t[1].y <= t[2].y) {
                // v0 <= v1 <= v2
                x0 = t[0].x; y0 = t[0].y;
                x1 = t[1].x; y1 = t[1].y;
                x2 = t[2].x; y2 = t[2].y;
            } else {
                // v0 <= v2 < v1
                x0 = t[0].x; y0 = t[0].y;
                x1 = t[2].x; y1 = t[2].y;
                x2 = t[1].x; y2 = t[1].y;
            }
        } else {
            // v2 < v0 < v1
            x0 = t[2].x; y0 = t[2].y;
            x1 = t[0].x; y1 = t[0].y;
            x2 = t[1].x; y2 = t[1].y;
        }
    } else {
        // v1 < v0
        if (t[2].y <= t[1].y) {
            // v2 <= v1 < v0
            x0 = t[2].x; y0 = t[2].y;
            x1 = t[1].x; y1 = t[1].y;
            x2 = t[0].x; y2 = t[0].y;
        } else {
            // v1 < v0, v1 < v2
            if (t[0].y <= t[2].y) {
                // v1 < v0 <= v2
                x0 = t[1].x; y0 = t[1].y;
                x1 = t[0].x; y1 = t[0].y;
                x2 = t[2].x; y2 = t[2].y;
            } else {
                // v1 < v2 < v0
                x0 = t[1].x; y0 = t[1].y;
                x1 = t[2].x; y1 = t[2].y;
                x2 = t[0].x; y2 = t[0].y;
            }
        }
    }
    t[0].x = x0; t[0].y = y0;
    t[1].x = x0; t[1].y = y1;
    t[2].x = x0; t[2].y = y2;
}

size_t verts_left_of(tri t, float b)
{
    size_t n = 0;
    for (size_t i = 0; i < 3; i++)
        if (t[i].x < b)
            n++;
        return n;
}

// It is unintuitive whether smaller y means above or below, so
// this function has a silly name.
size_t verts_smaller_y(tri t, float b)
{
    size_t n = 0;
    for (size_t i = 0; i < 3; i++)
        if (t[i].x < b)
            n++;
        return n;
}

float intersect_line_with_x(point *p0, point *p1, float x)
{
    assert(p0->x != p1->x);
    float m = (p1->y - p0->y) / (p1->x - p0->x);
    return p0->y + m * (x - p0->x);
}

float intersect_line_with_y(point *p0, point *p1, float y)
{
    assert(p0->y != p1->y);
    float m_inv = (p1->x - p0->x) / (p1->y - p0->y);
    return p0->x + m_inv * (y - p0->y);
}

size_t clip_tri_to_rect(const rect *bbox, tri tris[4])
{
    size_t n = 1;

    // clip against min.x
    for (size_t i = 0; i < n; i++) {
        point *t = tris[i];
        sort_tri_by_x(t);
        size_t n_clipped = verts_left_of(t, bbox->min.x);
        switch (n_clipped) {

        case 0:
            // triangle is unclipped.  Do nothing.
            break;

        case 1:
            // split into two triangles.
            assert(n < 4);
            point *nt = tris[n];
            nt[0].x = bbox->min.x;
            nt[0].y = intersect_line_with_x(&t[0], &t[1], bbox->min.x);
            nt[1]   = t[1];
            nt[2]   = t[2];
            t[0].y  = intersect_line_with_x(&t[0], &t[2], bbox->min.x);
            t[0].x  = bbox->min.x;
            n++;
            break;

        case 2:
            // move first two vertices to edge of box.
            t[0].y = intersect_line_with_x(&t[0], &t[2], bbox->min.x);
            t[0].x = bbox->min.x;
            t[1].y = intersect_line_with_x(&t[1], &t[2], bbox->min.x);
            t[1].x = bbox->min.x;
            break;

        case 3:
            // remove triangle completely.
            tris[i][0] = tris[n-1][0];
            tris[i][1] = tris[n-1][1];
            tris[i][2] = tris[n-1][2];
            i--; n--;
            break;

        default:
            assert(false);
        }
    }

    // clip against max.x.  Triangles are already sorted by x.
    for (size_t i = 0; i < n; i++) {
        point *t = tris[i];
        size_t n_clipped = 3 - verts_left_of(t, bbox->max.x);
        switch (n_clipped) {

        case 0:
            // triangle is unclipped.  Do nothing.
            break;

        case 1:
            // move last two vertices to edge of box.
            t[1].y = intersect_line_with_x(&t[0], &t[1], bbox->max.x);
            t[1].x = bbox->max.x;
            t[2].y = intersect_line_with_x(&t[0], &t[2], bbox->max.x);
            t[2].x = bbox->max.x;
            break;

        case 2:
            // split into two triangles.
            assert(n < 4);
            point *nt = tris[n];
            nt[0]   = t[0];
            nt[1]   = t[1];
            nt[2].x = bbox->max.x;
            nt[2].y = intersect_line_with_x(&t[0], &t[2], bbox->max.x);
            t[0].x  = bbox->max.x;
            t[0].y  = intersect_line_with_x(&t[1], &t[2], bbox->max.x);
            n++;
            break;

        case 3:
            // remove triangle completely.
            tris[i][0] = tris[n-1][0];
            tris[i][1] = tris[n-1][1];
            tris[i][2] = tris[n-1][2];
            i--; n--;
            break;

        default:
            assert(false);
        }
    }

    // clip against min.y
    for (size_t i = 0; i < n; i++) {
        point *t = tris[i];
        sort_tri_by_y(t);
        size_t n_clipped = verts_smaller_y(t, bbox->min.y);
        switch (n_clipped) {

        case 0:
            // triangle is unclipped.  Do nothing.
            break;

        case 1:
            // split into two triangles.
            assert(n < 4);
            point *nt = tris[n];
            nt[0].x = intersect_line_with_y(&t[0], &t[1], bbox->min.y);
            nt[0].y = bbox->min.y;
            nt[1]   = t[1];
            nt[2]   = t[2];
            t[0].x  = intersect_line_with_x(&t[0], &t[2], bbox->min.y);
            t[0].y  = bbox->min.y;
            n++;
            break;

        case 2:
            // move first two vertices to edge of box.
            t[0].x = intersect_line_with_x(&t[0], &t[2], bbox->min.y);
            t[0].y = bbox->min.y;
            t[1].x = intersect_line_with_x(&t[1], &t[2], bbox->min.y);
            t[1].y = bbox->min.y;
            break;

        case 3:
            // remove triangle completely.
            tris[i][0] = tris[n-1][0];
            tris[i][1] = tris[n-1][1];
            tris[i][2] = tris[n-1][2];
            i--; n--;
            break;

        default:
            assert(false);
        }
    }

    // clip against max.y.  Triangles are already sorted by x.
    for (size_t i = 0; i < n; i++) {
        point *t = tris[i];
        size_t n_clipped = 3 - verts_smaller_y(t, bbox->max.y);
        switch (n_clipped) {

        case 0:
            // triangle is unclipped.  Do nothing.
            break;

        case 1:
            // move last two vertices to edge of box.
            t[1].x = intersect_line_with_y(&t[0], &t[1], bbox->max.y);
            t[1].y = bbox->max.y;
            t[2].x = intersect_line_with_y(&t[0], &t[2], bbox->max.y);
            t[2].y = bbox->max.y;
            break;

        case 2:
            // split into two triangles.
            assert(n < 4);
            point *nt = tris[n];
            nt[0]   = t[0];
            nt[1]   = t[1];
            nt[2].x = intersect_line_with_y(&t[0], &t[2], bbox->max.y);
            nt[2].y = bbox->max.y;
            t[0].x  = intersect_line_with_y(&t[1], &t[2], bbox->max.y);
            t[0].y  = bbox->max.y;
            n++;
            break;

        case 3:
            // remove triangle completely.
            tris[i][0] = tris[n-1][0];
            tris[i][1] = tris[n-1][1];
            tris[i][2] = tris[n-1][2];
            i--; n--;
            break;

        default:
            assert(false);
        }
    }

    return n;
}
