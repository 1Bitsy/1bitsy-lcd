#ifndef COORD_included
#define COORD_included

#include <assert.h>

// A coordinate is a signed 24.8 fixed-point integer.

typedef int32_t coord;
static const coord COORD_MIN = (coord)INT32_MIN;
static const coord COORD_MAX = (coord)INT32_MAX;

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

static inline coord coord_rfrac(coord x)
{
    // frac(x) + rfrac(x) == 1
    return int_to_coord(1) - coord_frac(x);
}

static inline coord coord_round(coord x)
{
    return (x + 0x80) & 0xFFFFFF00;
}

#endif /* !COORD_included */
