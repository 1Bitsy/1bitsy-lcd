#ifndef GFX_TYPES_included
#define GFX_TYPES_included

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

typedef uint16_t gfx_rgb565;
typedef uint32_t gfx_rgb888;
typedef uint8_t  gfx_alpha8;

typedef struct gfx_pixtile gfx_pixtile;

typedef union gfx_point {
    struct {
        float x, y;             // use p.x, p.y
    };
    float c[2];                 // use p.c[i]
} gfx_point;

typedef struct gfx_trapezoid {
    float xl0, xr0, y0;
    float xl1, xr1, y1;
} gfx_trapezoid;

typedef struct gfx_triangle {
    gfx_point v[3];
} gfx_triangle;

#endif /* !GFX_TYPES_included */
