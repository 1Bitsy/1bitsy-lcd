#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <gfx.h>
#include <lcd.h>
#include <math-util.h>
#include <systick.h>
#include <touch.h>

#include "toggle-button.h"

// Control two traffic lights using two touchscreen buttons.

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define BLACK_565  0x0000
#define WHITE_565  0xFFFF
#define GRAY50_565 0x7BEF
#define GRAY88_565 0xE71C
#define RED_565    0xF800
#define GREEN_565  0x07E0
#define BLUE_565   0x001F

#define BG_COLOR   GRAY88_565
#define STOPLIGHT_COLOR 0x7BE0

uint32_t fps;

static gfx_button buttons[2];
static const size_t button_count = 2;

static void init_buttons(void)
{
    gfx_init_toggle_button(&buttons[0], false, (gfx_ipoint){{  40, 250 }});
    gfx_init_toggle_button(&buttons[1], false, (gfx_ipoint){{ 140, 250 }});
}

static bool        is_touching;
static gfx_button *touched_button;
static bool        was_down;

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);
    flash_prefetch_enable();
    flash_icache_enable();
    flash_dcache_enable();

    setup_systick(MY_CLOCK.ahb_frequency);

    init_buttons();

    lcd_set_bg_color(BG_COLOR, false);
    lcd_init();

    touch_init();
}

static void animate(void)
{
    bool was_touching = is_touching;
    is_touching = touch_count() >= 1 ? true : false;
    if (is_touching) {
        gfx_ipoint tp = touch_point(0);
        if (!was_touching) {
            // Finger down event.  Find out what we hit.
            for (size_t i = 0; i < button_count; i++) {
                gfx_button *bp = &buttons[i];
                if (gfx_point_is_in_button(&tp, bp)) {
                    touched_button = bp;
                    was_down = bp->is_down;
                    bp->is_down = !was_down;
                    break;
                }
            }
        } else {
            // Is finger still on button?
            gfx_button *btn = touched_button;
            if (btn)
                btn->is_down = was_down ^ gfx_point_is_in_button(&tp, btn);
        }
    } else if (was_touching) {
        // Finger up event.
        touched_button = NULL;
    }
}

// XXX this should be a gfx primitive.
static void fill_circle(gfx_pixtile *tile,
                        gfx_point *const center,
                        float radius,
                        gfx_rgb565 color)
{
    int x = radius;
    int y = 0;
    int err = 0;
    while (x >= y) {
        gfx_fill_span(tile, center->x - x, center->x + x, center->y + y, color);
        if (y)
            gfx_fill_span(tile,
                          center->x - x, center->x + x, center->y - y,
                          color);
        if (err <= 0) {
            y++;
            err += 2 * y + 1;
        }
        if (err > 0) {
            --x;
            if (x > y) {
            gfx_fill_span(tile,
                          center->x - y, center->x + y, center->y - x,
                          color);
            gfx_fill_span(tile,
                          center->x - y, center->x + y, center->y + x,
                          color);
            }
            err -= 2 * x + 1;
        }
    }
}

static void draw_stoplight(gfx_pixtile *tile, int x, int y, bool go)
{
    // cheap stoplight: yellow rectangle, two circles.

    // outline rectangle
    gfx_draw_line(tile, x - 30, y - 60, x + 30, y - 60, BLACK_565);
    gfx_draw_line(tile, x - 30, y - 60, x - 30, y + 60, BLACK_565);
    gfx_draw_line(tile, x - 30, y + 60, x + 30, y + 60, BLACK_565);
    gfx_draw_line(tile, x + 30, y - 60, x + 30, y + 60, BLACK_565);

    // fill rectangle
    for (int iy = y - 59; iy < y + 60; iy++)
        gfx_fill_span(tile, x - 29, x + 30, iy, STOPLIGHT_COLOR);

    // red light
    gfx_point stop_center = {{ .x = x, .y = y - 30 }};
    fill_circle(tile, &stop_center, 20, go ? GRAY50_565 : RED_565);

    // green light
    gfx_point go_center = {{ .x = x, .y = y + 30 }};
    fill_circle(tile, &go_center, 20, go ? GREEN_565 : GRAY50_565);
}

// N.B., lower level functions handle all the clipping.  We just
// render the whole scene (such as it is) on every tile here.
static void draw_tile(gfx_pixtile *tile)
{
    for (size_t i = 0; i < button_count; i++)
        draw_stoplight(tile, 70 + i * 100, 140, buttons[i].is_down);
    for (size_t i = 0; i < button_count; i++)
        gfx_draw_button(tile, &buttons[i]);
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
