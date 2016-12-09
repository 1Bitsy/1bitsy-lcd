#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

#include <gfx.h>
#include <lcd.h>
#include <math-util.h>
#include <systick.h>
#include <touch.h>

// Draw various lines and display using big pixels.

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

#define ZOOM 10
#define LEFT (LCD_WIDTH / 4)
#define RIGHT (LCD_WIDTH * 3 / 4)
#define TOP (LCD_HEIGHT / 4)
#define BOTTOM (LCD_HEIGHT * 3 / 4)
#define WIDTH ((RIGHT - LEFT) / ZOOM)
#define HEIGHT ((BOTTOM - TOP) / ZOOM)

#define TOUCH_PROXIMITY 20      // pixel distance to match
#define CROSSHAIR_RADIUS 64

static gfx_pixtile my_tile;
static gfx_rgb565 my_pixels[HEIGHT * WIDTH];

uint32_t fps;
static gfx_point line_p0;
static gfx_point line_p1;
static bool is_touching;
static int endpt;
static gfx_point tp_offset;

static inline gfx_point zoom_in(gfx_point p)
{
    return (gfx_point) { .x = LEFT + ZOOM * p.x, .y = TOP + ZOOM * p.y };
}

static inline gfx_point zoom_out(gfx_point p)
{
    return (gfx_point) { .x = (p.x - LEFT) / ZOOM, .y = (p.y - TOP) / ZOOM };
}

static inline float distance2(gfx_point *a, gfx_point *b)
{
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    return dx * dx + dy + dy;
}

static void render_tile(void)
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
            *gfx_pixel_address_unchecked(&my_tile, x, y) = FG_COLOR;

    gfx_point p0 = zoom_out(line_p0);
    gfx_point p1 = zoom_out(line_p1);
    gfx_draw_line(&my_tile,
                  p0.x, p0.y,
                  p1.x, p1.y,
                  PIX_COLOR);
}

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

    gfx_init_pixtile(&my_tile, my_pixels, 0, 0, WIDTH, HEIGHT, WIDTH);

    line_p0 = zoom_in((gfx_point){ .x = +1,        .y = +1         });
    line_p1 = zoom_in((gfx_point){ .x = WIDTH - 2, .y = HEIGHT - 2 });
    render_tile();

}

static void animate(void)
{
    bool was_touching = is_touching;
    is_touching = touch_count() != 0;
    if (!is_touching)
        return;

    gfx_ipoint itp = touch_point(0);
    gfx_point tp = (gfx_point) { .x = itp.x, .y = itp.y };

    if (!was_touching) {
        // on finger down, decide which endpoint to drag and
        // calculate an offset.
        float dd0 = distance2(&tp, &line_p0);
        float dd1 = distance2(&tp, &line_p1);
        float prox2 = TOUCH_PROXIMITY * TOUCH_PROXIMITY;

        if (dd0 > prox2 && dd1 > prox2) {
            endpt = -1;
            return;
        }
        if (dd0 < dd1) {
            endpt = 0;
            tp_offset = (gfx_point) {
                .x = line_p0.x - tp.x,
                .y = line_p0.y - tp.y
            };
        } else {
            endpt = 1;
            tp_offset = (gfx_point) {
                .x = line_p1.x - tp.x,
                .y = line_p1.y - tp.y
            };
        }
    } else {
        // on drag, move the active endpoint.
        if (endpt == 0) {
            line_p0 = (gfx_point) { {
                .x = CLAMP(0, LCD_WIDTH - 1, tp.x + tp_offset.x),
                .y = CLAMP(0, LCD_HEIGHT - 1, tp.y + tp_offset.y),
            } };
        } else if (endpt == 1) {
            line_p1 = (gfx_point) { {
                .x = CLAMP(0, LCD_WIDTH, tp.x + tp_offset.x),
                .y = CLAMP(0, LCD_HEIGHT, tp.y + tp_offset.y),
            } };
        }
    }
    
    render_tile();
}

static void fill_rect(gfx_pixtile *tile,
                      int x, int y,
                      size_t w, size_t h,
                      gfx_rgb565 color)
{
    for (int iy = y; iy < (ssize_t)(y + h); iy++)
        gfx_fill_span(tile, x, x + w, iy, color);
}

static void fill_diamond(gfx_pixtile *tile,
                         gfx_point center,
                         float radius,
                         gfx_rgb565 color)
{
    int w = 1;
    int xc = ROUND(center.x);
    for (int iy = CEIL(center.y - radius); iy < center.y; iy++) {
        gfx_fill_span(tile, xc - w, xc + w, iy, color);
        w++;
    }
    for (int iy = center.y; iy <= center.y + radius; iy++) {
        --w;
        gfx_fill_span(tile, xc - w, xc + w, iy, color);
    }
}

static void draw_crosshairs(gfx_pixtile *tile,
                            gfx_point    center,
                            gfx_rgb565   color)
{
    const float x = center.x;
    const float y = center.y;
    const float r = CROSSHAIR_RADIUS;
    gfx_draw_line(tile, x - r, y, x + r, y, color);
    gfx_draw_line(tile, x, y - r, x, y + r, color);
}

static void draw_tile(gfx_pixtile *tile)
{
    int iy0 = MAX(0, (tile->y - TOP) / ZOOM);
    int iy1 = MIN(HEIGHT, (ssize_t)(tile->y + tile->h - TOP + ZOOM - 1) / ZOOM);

    for (int iy = iy0; iy < iy1; iy++) {
        int y = TOP + iy * ZOOM;
        for (int ix = 0; ix < WIDTH; ix++) {
            int x = LEFT + ix * ZOOM;
            gfx_rgb565 color = *gfx_pixel_address_unchecked(&my_tile, ix, iy);
            fill_rect(tile, x + 1, y + 1, ZOOM - 1, ZOOM - 1, color);
        }
    }

    gfx_draw_line(tile, line_p0.x, line_p0.y, line_p1.x, line_p1.y, LINE_COLOR);
    fill_diamond(tile, line_p0, ZOOM/2, P0_COLOR);
    fill_diamond(tile, line_p1, ZOOM/2, P1_COLOR);
    if (is_touching) {
        if (endpt == 0)
            draw_crosshairs(tile, line_p0, P0_COLOR);
        else if (endpt == 1)
            draw_crosshairs(tile, line_p1, P1_COLOR);
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

void calc_fps(void);
void calc_fps(void)
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
