#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <gfx.h>
#include <lcd.h>
#include <systick.h>
#include <math-util.h>

// The classic Munching Square eye candy.

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])
#define BG_COLOR 0x0000         // black
#define MAGIC 27                // try different values

uint32_t   fps;
gfx_rgb565 base_color;

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
    base_color += 0x0021;
}

static void draw_tile(gfx_pixtile *tile)
{
    const int y_off = 32;
    const int x_off = -8;

    int y0 = MAX(0, tile->y - y_off);
    int y1 = MIN(256, tile->y + (int)tile->h - y_off);
    int x0 = tile->x - x_off;
    int x1 = x0 + tile->w - x_off;
    gfx_rgb565 base = base_color;
    for (int y = y0; y < y1; y++) {
        gfx_rgb565 *p =
            gfx_pixel_address_unchecked(tile, x0 + x_off, y + y_off);
        for (int x = x0; x < x1; x++)
            *p++ = base + MAGIC * (x ^ y);
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

static void calc_fps(void)
{
    static uint32_t next_time;
    static uint32_t frame_count;
    frame_count++;
    if (system_millis >= next_time) {
        fps = frame_count;
        frame_count = 0;
        next_time += 1000;
    }
}

static void run(void)
{
    while (true) {
        animate();
        draw_frame();
        calc_fps();
    }
}

int main(void)
{
    setup();
    run();
}

