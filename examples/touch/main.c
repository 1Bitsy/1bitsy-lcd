#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <gfx.h>
#include <lcd.h>
#include <math-util.h>
#include <systick.h>
#include <touch.h>

// Move crosshairs around using touch.  Works with one or two fingers.

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define BLACK_565  0x0000
#define WHITE_565  0xFFFF
#define GRAY50_565 0x7BEF
#define RED_565    0xF800
#define GREEN_565  0x07E0
#define BLUE_565   0x001F

#define FG_COLOR   WHITE_565
#define BG_COLOR   GRAY50_565
#define PIX_COLOR  BLACK_565
#define LINE_COLOR GREEN_565
#define P0_COLOR   RED_565
#define P1_COLOR   BLUE_565

#define CH0_COLOR  RED_565
#define CH1_COLOR  BLUE_565

#define CROSSHAIR_RADIUS 64

uint32_t fps;
gfx_point line_p0;
gfx_point line_p1;

bool is_touching;
unsigned t_count;
gfx_ipoint touch_pt0;
gfx_ipoint touch_pt1;

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);
    flash_prefetch_enable();
    flash_icache_enable();
    flash_dcache_enable();

    setup_systick(MY_CLOCK.ahb_frequency);

    lcd_set_bg_color(BG_COLOR, false);
    lcd_init();

    touch_init();
}

static void animate(void)
{
    t_count = touch_count();
    if (t_count >= 1) {
        gfx_ipoint tp = touch_point(0);
        touch_pt0 = (gfx_ipoint){ .x = tp.x, .y = tp.y };
    }
    if (t_count >= 2) {
        gfx_ipoint tp = touch_point(1);
        touch_pt1 = (gfx_ipoint){ .x = tp.x, .y = tp.y };
    }
}

static void draw_tile(gfx_pixtile *tile)
{
    if (t_count >= 1) {
        const float      x = touch_pt0.x;
        const float      y = touch_pt0.y;
        const float      r = CROSSHAIR_RADIUS;
        const gfx_rgb565 c = CH0_COLOR;
        gfx_draw_line(tile, x - r, y, x + r, y, c);
        gfx_draw_line(tile, x, y - r, x, y + r, c);
    }
    if (t_count >= 2) {
        const float      x = touch_pt1.x;
        const float      y = touch_pt1.y;
        const float      r = CROSSHAIR_RADIUS;
        const gfx_rgb565 c = CH1_COLOR;
        gfx_draw_line(tile, x - r, y, x + r, y, c);
        gfx_draw_line(tile, x, y - r, x, y + r, c);
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
