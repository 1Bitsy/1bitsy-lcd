#include <stdlib.h>

#include "ILI9341.h"
#include "systick.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define LED_PORT GPIOA
#define LED_PIN GPIO8
#define LED_RCC_PORT RCC_GPIOA


ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

static void heartbeat(uint32_t msec_time)
{
    // if (msec_time & 0x400)
    if (msec_time / 1000 & 1)
        gpio_set(LED_PORT, LED_PIN);
    else
        gpio_clear(LED_PORT, LED_PIN);
}

static void setup_heartbeat(void)
{
    rcc_periph_clock_enable(LED_RCC_PORT);
    gpio_mode_setup(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, LED_PIN);
    register_systick_handler(heartbeat);
}

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);

    setup_heartbeat();

    my_ILI.begin();
}

static uint16_t colors[] = {
    // ILI9341_BLACK,
    ILI9341_NAVY,
    ILI9341_DARKGREEN,
    ILI9341_DARKCYAN,
    ILI9341_MAROON,
    ILI9341_PURPLE,
    ILI9341_OLIVE,
    ILI9341_LIGHTGREY,
    ILI9341_DARKGREY,
    ILI9341_BLUE,
    ILI9341_GREEN,
    ILI9341_CYAN,
    ILI9341_RED,
    ILI9341_MAGENTA,
    ILI9341_YELLOW,
    ILI9341_WHITE,
    ILI9341_ORANGE,
    ILI9341_GREENYELLOW,
    ILI9341_PINK,
};
static size_t color_count = (&colors)[1] - colors;

//                     --  0
//                     |
//               +     -- y0
//               |\    |
//      +--------+ \   -- y1
//      |           +  -- y2
//      +--------+ /   -- y3
//               |/    |
//               +     -- y4
//                     |
//  |---|--------|--|--+- y5
//  0   x0       x1 x2 x3

#define arrow_x0  32
#define arrow_x1  96
#define arrow_x2 128
#define arrow_x3 160
#define arrow_y0  (64-16-16)
#define arrow_y1  (64-16)
#define arrow_y2  64
#define arrow_y3  (64+16)
#define arrow_y4  (64+16+16)
#define arrow_y5 128

int frame_stats[4];

static uint16_t buf[arrow_y5][arrow_x3];
// static uint16_t (*buf)[arrow_x3] = (uint16_t (*)[arrow_x3])0x20000000;

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t color)
{
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            buf[y][x] = color;
}


static uint16_t pix_color(int x, int y, int offset)
{
    return colors[(x + abs(y - arrow_y2) - offset) % color_count];
}

static void run()
{
    fill_rect(0, 0, arrow_x3, arrow_y5, ILI9341_BLACK);
    for (int x = 0; x < arrow_x3; x++) {
        uint8_t r = x * 255 / arrow_x3;
        buf[0][x] = my_ILI.color565(r, 0, 0);
        buf[arrow_y5 - 1][x] = my_ILI.color565(r, 255, 0);
    }
    for (int y = 0; y < arrow_y5; y++) {
        uint8_t g = y * 255 / arrow_y5;
        buf[y][0] = my_ILI.color565(0, g, 0);
        buf[y][arrow_x3 - 1] = my_ILI.color565(255, g, 0);
    }

    uint32_t rot_time = 1000;
    uint8_t rot = 0;
    int frame_count = 0;

    for (int ci = 0; ; ci = (ci + 1) % color_count) {
        for (int y = arrow_y1; y < arrow_y3; y++)
            for (int x = arrow_x0; x < arrow_x1; x++)
                buf[y][x] = pix_color(x, y, ci);
        for (int y = arrow_y0; y < arrow_y2; y++)
            for (int x = arrow_x1; x < arrow_x2 - (arrow_y2 - y); x++)
                buf[y][x] = pix_color(x, y, ci);
        for (int y = arrow_y2; y < arrow_y4; y++)
            for (int x = arrow_x1; x < arrow_x2 - (y - arrow_y2); x++)
                buf[y][x] = pix_color(x, y, ci);
#define DELAY_60Hz
#ifdef DELAY_60Hz
        uint32_t deadline = rot_time - 1000;
        deadline += frame_count * 1000 / 60;
        while (system_millis < deadline)
            continue;
#endif
        my_ILI.writeRect(0, 0, arrow_x3, arrow_y5, buf[0]);
        frame_count++;
        if (system_millis >= rot_time) {
            rot_time += 1000;
            // my_ILI.fillRect(0, 0, arrow_x3, arrow_y5, ILI9341_BLACK);
            my_ILI.fillRect(1, 1, arrow_x3 - 2, arrow_y5 - 2, ILI9341_BLACK);
            frame_stats[rot] = frame_count;
            frame_count = 0;
            rot = (rot + 1) % 4;
            my_ILI.setRotation(rot);
            my_ILI.fillRect(0, 0, arrow_x3, arrow_y5, ILI9341_BLACK);
        }
    }
}

int main(void)
{
    setup();
    run();
}
