#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <gfx.h>
#include <lcd.h>
#include <systick.h>
#include <math-util.h>

// This is about as simple as a libgfx client can be.
// Draw a rectangular grid of pixels over and over.
// Leave a moving circle of pixels uncolored.
//
// The basic structure is universal, though:
//   main() sets up, then runs.
//   run() repeatedly animates and draws a frame.
//   animate() updates some state.
//   draw_frame() splits the screen into tiles and draws each.
//   draw_tile() calls gfx drawing functions.

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])
#define FG_COLOR 0xFFE0         // yellow
#define BG_COLOR 0x001F         // blue

static int center_y = 160;

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);
    flash_prefetch_enable();
    flash_icache_enable();
    flash_dcache_enable();

    setup_systick(MY_CLOCK.ahb_frequency);

    lcd_set_bg_color(BG_COLOR, false);
    lcd_init();
}

static void animate(void)
{
    // Bounce the circle center up and down.

    static int inc = +1;
    center_y += inc;
    if (center_y > 180) {
        center_y = 180;
        inc = -1;
    }
    if (center_y < 140) {
        center_y = 140;
        inc = +1;
    }
}

static bool is_in_circle(int x, int y)
{
    x -= 120;
    y -= center_y;
    return x * x + y * y <= 50 * 50;
}

static void draw_tile(gfx_pixtile *tile)
{
    bool in_circle = false;

    for (int y = 80; y < 240; y++) {
        for (int x = 60; x < 180; x++) {
            in_circle = is_in_circle(x, y);
            if (!in_circle || !((x % 3) && !(y % 2)))
                gfx_fill_pixel(tile, x, y, FG_COLOR);
        }
    }
}

static void draw_frame(void)
{
    size_t h;

    for (size_t y = 0; y < LCD_HEIGHT; y += h) {
        h = MIN(LCD_MAX_TILE_ROWS, LCD_HEIGHT - y);
        gfx_pixtile *tile = lcd_alloc_pixtile(0, y, LCD_WIDTH, h);
        draw_tile(tile);
        lcd_send_pixtile(tile);
    }

}

static void run(void)
{
    while (true) {
        animate();
        draw_frame();
    }
}

int main(void)
{
    setup();
    run();
}

