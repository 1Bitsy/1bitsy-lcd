#ifndef BUTTON_included
#define BUTTON_included

#include <stdbool.h>

#include "gfx-pixtile.h"

typedef struct gfx_button {
    bool               is_down;
    gfx_ipoint         position;
    const gfx_pixtile *up_image;
    const gfx_pixtile *down_image;
} gfx_button;

extern void gfx_button_init(gfx_button *,
                            bool is_down,
                            gfx_ipoint position,
                            const gfx_pixtile *up_image,
                            const gfx_pixtile *down_image);

extern bool gfx_point_is_in_button(const gfx_ipoint *, const gfx_button *);

extern void gfx_draw_button(gfx_pixtile *, const gfx_button *);

#endif /* !BUTTON_included */
