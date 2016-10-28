#ifndef VIDEO_included
#define VIDEO_included

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "coord.h"

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define PIXTILE_WIDTH SCREEN_WIDTH
#define PIXTILE_MAX_HEIGHT 136

typedef struct pixtile {
    uint16_t (*pixels)[PIXTILE_WIDTH];
    size_t     y;
    size_t     height;
} pixtile;

extern void     setup_video        (void);
extern pixtile *alloc_pixtile      (size_t y, size_t h);
extern void     send_pixtile       (pixtile *);

extern void     video_set_bg_color (uint16_t color, bool immediate);
// if immediate, cleared tiles will be re-cleared in new color.
extern uint16_t video_bg_color     (void);

static inline bool x_in_pixtile(const pixtile *tile, int x)
{
    (void)tile;
    return 0 <= x && x < PIXTILE_WIDTH;
}

static inline bool y_in_pixtile(const pixtile *tile, int y)
{
    return (int)tile->y <= y && y < (int)(tile->y + tile->height);
}

#endif /* !VIDEO_included */
