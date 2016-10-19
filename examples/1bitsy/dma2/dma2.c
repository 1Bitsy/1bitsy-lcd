#include "video.h"

#include <libopencm3/stm32/rcc.h>

#include "systick.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define MIN(a, b) ({                            \
                      __typeof__ (a) a_ = (a);  \
                      __typeof__ (b) b_ = (b);  \
                      a_ < b_ ? a_ : b_;        \
                  })
#define MAX(a, b) ({                            \
                      __typeof__ (a) a_ = (a);  \
                      __typeof__ (b) b_ = (b);  \
                      a_ > b_ ? a_ : b_;        \
                  })

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);
    // setup_heartbeat();

    setup_video();
}

static void animate(void)
{}

static void draw_tile(pixtile *tile)
{
    (void)tile;
    static size_t y0 = 0;
    static int inc = +1;
    size_t y1 = y0 + 240;
    for (size_t y = tile->y; y < tile->y + tile->height; y++) {
        if (y >= y0 && y < y1) {
            tile->pixels[y - tile->y][y - y0] = 0x0000;
            tile->pixels[y - tile->y][239 - y + y0] = 0x0000;
        }
    }

    if (y0 == 0 && inc < 0) {
        y0 = 1;
        inc = +1;
    }
    y0 += inc;
    if (y0 >= 320 - 240) {
        y0 = 320 - 240 - 1;
        inc = -1;
    }
    video_set_bg_color(0xFFFF);
}

static void draw_frame(void)
{
    size_t h;

    for (size_t y = 0; y < SCREEN_HEIGHT; y += h) {
        h = MIN((size_t)PIXTILE_MAX_HEIGHT, SCREEN_HEIGHT - y);
        pixtile *tile = alloc_pixtile(y, h);
        draw_tile(tile);
        send_pixtile(tile);
    }
}

static void run(void)
{
    while (1) {
        animate();
        draw_frame();
    }
}

int main(void)
{
    setup();
    run();
}
