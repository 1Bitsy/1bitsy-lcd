#ifndef VIDEO_included
#define VIDEO_included

#include <stddef.h>
#include <stdint.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define PIXTILE_WIDTH SCREEN_WIDTH
#define PIXTILE_MAX_HEIGHT 136

typedef struct pixtile {
    uint16_t    (*pixels)[PIXTILE_WIDTH];
    size_t        y;
    size_t        height;
    int           state;
} pixtile;

extern void     setup_video        (void);
extern pixtile *alloc_pixtile      (size_t y, size_t h);
extern void     send_pixtile       (pixtile *);

extern void     video_set_bg_color (uint16_t);
extern uint16_t video_bg_color     (void);
// N.B.,The background color will not change immediately.

#endif /* !VIDEO_included */
