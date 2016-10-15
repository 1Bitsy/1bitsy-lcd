// Feature switches
#define BG_DEBUG

#include <assert.h>
#include <string.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>

#include <libopencm3/stm32/rcc.h>

#include "ILI9341.h"
#include "intr.h"
#include "systick.h"

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

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define LED_PORT GPIOA
#define LED_PIN GPIO8
#define LED_RCC_PORT RCC_GPIOA

#define RAM_SIZE (128 << 10)    // 128 Kbytes
#define PIXBUF_COUNT 2
#define PIXBUF_SIZE_BYTES (RAM_SIZE / PIXBUF_COUNT)

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define TILE_WIDTH SCREEN_WIDTH
#define TILE_MAX_HEIGHT (PIXBUF_SIZE_BYTES / (TILE_WIDTH * sizeof (uint16_t)))

#define BG_COLOR 0x07ff

#ifdef BG_DEBUG

static inline uint16_t calc_bg_color(void)
{
    static int frame;
    uint32_t r = frame % 3 == 0 ? 0xFF : frame & 0xFF;
    uint32_t g = frame % 3 == 1 ? 0xFF : frame & 0xFF;
    uint32_t b = frame % 3 == 2 ? 0xFF : frame & 0xFF;
    uint32_t color = (r & 0xF8)  << 8 | (g & 0xFC) << 3 | b >> 3;
    frame++;
    return color;
}
#endif

// Tile is virtual.  It refers to a part of the screen.
// Pixbuf is physical.  It refers to a part of RAM.

typedef struct tile tile;
typedef struct pixbuf pixbuf;

struct tile {
    uint16_t (*pixels)[TILE_WIDTH];
    size_t     height;
    size_t     y;
    pixbuf    *pix;
};

static inline size_t tile_size_bytes(tile *tp)
{
    assert(sizeof *tp->pixels == 480); // XXX
    assert(false);
    return tp->height * sizeof *tp->pixels;
}

typedef enum pixbuf_state {
    PS_CLEARED,
    PS_DRAWING,
    PS_SEND_WAIT,
    PS_SENDING,
    PS_CLEAR_WAIT,
    PS_CLEARING,
} pixbuf_state;

struct pixbuf {
    void *base;
    volatile pixbuf_state state;
};

static pixbuf pixbufs[PIXBUF_COUNT];

ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

volatile int fps;
static volatile uint32_t dma_intr_count;
static volatile bool clear_dma_busy;

#define CLEAR_DMA
#ifdef CLEAR_DMA

// static void clear_tile(tile *tp, uint16_t color) // XXX deprecated
// {
//     size_t size = tile_size_bytes(tp);
//     uintptr_t base = (uintptr_t)tp->base;
//     assert(0x20000000 <= base);
//     assert(base + size <= 0x20020000);
//     assert(size >= 16);
//     assert(!(size & 0xF));      // multiple of 16 bytes

//     // Fill the first 16 bytes with the pixel.  Then
//     // DMA will duplicate that through the buffer.
//     uint32_t *p = (uint32_t *)tp->base;
//     const int pburst = 4;
//     uint32_t pix_twice = (uint32_t)color << 16 | color;
//     for (int i = 0; i < pburst; i++)
//         *p++ = pix_twice;

//     uint32_t ic = dma_intr_count;

//     DMA2_S7CR &= ~DMA_SxCR_EN;
//     while (DMA2_S7CR & DMA_SxCR_EN)
//         continue;

//     DMA2_S7PAR  = tp->base;
//     DMA2_S7M0AR = p;
//     DMA2_S7NDTR = size - 16;
//     DMA2_S7FCR  = (DMA_SxFCR_FEIE          |
//                    DMA_SxFCR_DMDIS         |
//                    DMA_SxFCR_FTH_4_4_FULL);
//     DMA2_S7CR   = (DMA_SxCR_CHSEL_0        |
//                    DMA_SxCR_MBURST_INCR4   |
//                    DMA_SxCR_PBURST_INCR4   |
//                    DMA_SxCR_PL_LOW         |
//                    DMA_SxCR_MSIZE_32BIT    |
//                    DMA_SxCR_PSIZE_32BIT    |
//                    DMA_SxCR_MINC           |
//                    DMA_SxCR_DIR_MEM_TO_MEM |
//                    DMA_SxCR_TCIE           |
//                    DMA_SxCR_TEIE           |
//                    DMA_SxCR_DMEIE          |
//                    DMA_SxCR_EN);

//     while (dma_intr_count == ic)
//         continue;

//     // dma2_s7cr   = DMA2_S7CR;
//     // dma2_s7ndtr = DMA2_S7NDTR;
//     // dma2_s7par  = DMA2_S7PAR;
//     // dma2_s7m0ar = DMA2_S7M0AR;
//     // dma2_s7fcr  = DMA2_S7FCR;
// }

static void start_clear_dma(pixbuf *pix)
{
    assert(PIXBUF_SIZE_BYTES == 65536); // XXX
    size_t size = PIXBUF_SIZE_BYTES;
    uintptr_t base = (uintptr_t)pix->base;
    assert(0x20000000 <= base);
    assert(base + size <= 0x20020000);
    assert(size >= 16);
    assert(!(size & 0xF));      // multiple of 16 bytes

    // Fill the first 16 bytes with the pixel.  Then
    // DMA will duplicate that through the buffer.
    uint32_t *p = (uint32_t *)pix->base;
    const int pburst = 4;

#ifdef BG_DEBUG    
    uint32_t color = calc_bg_color();
    uint32_t pix_twice = color << 16 | color;
#else
    uint32_t pix_twice = BG_COLOR << 16 | BG_COLOR;
#endif

    for (int i = 0; i < pburst; i++)
        *p++ = pix_twice;

    DMA2_S7CR &= ~DMA_SxCR_EN;
    while (DMA2_S7CR & DMA_SxCR_EN)
        continue;

    DMA2_S7PAR  = pix->base;
    DMA2_S7M0AR = p;
    DMA2_S7NDTR = (size - 16) / 4;
    DMA2_S7FCR  = (DMA_SxFCR_FEIE          |
                   DMA_SxFCR_DMDIS         |
                   DMA_SxFCR_FTH_4_4_FULL);
    DMA2_S7CR   = (DMA_SxCR_CHSEL_0        |
                   DMA_SxCR_MBURST_INCR4   |
                   DMA_SxCR_PBURST_INCR4   |
                   DMA_SxCR_PL_LOW         |
                   DMA_SxCR_MSIZE_32BIT    |
                   DMA_SxCR_PSIZE_32BIT    |
                   DMA_SxCR_MINC           |
                   DMA_SxCR_DIR_MEM_TO_MEM |
                   DMA_SxCR_TCIE           |
                   DMA_SxCR_TEIE           |
                   DMA_SxCR_DMEIE          |
                   DMA_SxCR_EN);
}

void dma2_stream7_isr(void)
{
    dma_intr_count++;

    const uint32_t ERR_BITS = DMA_HISR_TEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_FEIF7;
    const uint32_t CLEAR_BITS = DMA_HISR_TCIF7 | DMA_HISR_HTIF7 | ERR_BITS;
    uint32_t dma2_hisr = DMA2_HISR;
    DMA2_HIFCR = dma2_hisr & CLEAR_BITS;
    assert((dma2_hisr & ERR_BITS) == 0);

    clear_dma_busy = false;
    for (size_t i = 0; i < PIXBUF_COUNT; i++) {
        pixbuf *pix = &pixbufs[i];
        if (pix->state == PS_CLEARING) {
            pix->state = PS_CLEARED;
        } else if (pix->state == PS_CLEAR_WAIT && !clear_dma_busy) {
            pix->state = PS_CLEARING;
            clear_dma_busy = true;
            start_clear_dma(pix);
        }
    }
}

static void clear_pixbuf(pixbuf *pix)
{
    bool busy;
    WITH_INTERRUPTS_MASKED {
        busy = clear_dma_busy;
        if (busy) {
            pix->state = PS_CLEAR_WAIT;
        } else {
            clear_dma_busy = true;
            pix->state = PS_CLEARING;
        }
    }
    if (!busy)
        start_clear_dma(pix);
}

#else // !CLEAR_DMA

// static void clear_tile(tile *tp, uint16_t color) // XXX deprecated
// {
//     uint32_t pix_twice = (uint32_t)color << 16 | color;
//     uint32_t *p = (uint32_t *)tp->base;
//     size_t n = tile_size_bytes(tp) / sizeof *p;
//     for (size_t i = 0; i < n; i++)
//         *p++ = pix_twice;
// }

static void clear_pixbuf(pixbuf *pix)
{
#define BG_DEBUG
#ifdef BG_DEBUG    
    uint32_t color = calc_bg_color();
    uint32_t pix_twice = color << 16 | color;
#else
    uint32_t pix_twice = BG_COLOR << 16 | BG_COLOR;
#endif

    uint32_t *p = (uint32_t *)pix->base;
    size_t n = PIXBUF_SIZE_BYTES / sizeof *p;
    for (size_t i = 0; i < n; i++)
        *p++ = pix_twice;
    pix->state = PS_CLEARED;
}

#endif // CLEAR_DMA

#undef VIDEO_DMA
#ifdef VIDEO_DMA

#error "Write me!"

#else

static void send_tile(tile *tp)
{
    my_ILI.writeRect(0, tp->y, TILE_WIDTH, tp->height, tp->pixels[0]);
    clear_pixbuf(tp->pix);
}

#endif

static void heartbeat(uint32_t msec_time)
{
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

static void setup_pixbufs(void)
{
    size_t pixbuf_size = RAM_SIZE / PIXBUF_COUNT;
    for (size_t i = 0; i < PIXBUF_COUNT; i++) {
        pixbufs[i].base = (void *)(0x20000000 + i * pixbuf_size);
        pixbufs[i].state = PS_CLEAR_WAIT;
        clear_pixbuf(&pixbufs[i]);
    }
}

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);

    setup_heartbeat();

    rcc_periph_clock_enable(RCC_DMA2);
    nvic_enable_irq(NVIC_DMA2_STREAM7_IRQ);
    // dma2_hisr_addr = &DMA2_HISR;
    // dma2_s7cr_addr = &DMA2_S7CR;
    // dma2_s7ndtr_addr = &DMA2_S7NDTR;
    // dma2_s7par_addr = &DMA2_S7PAR;
    // dma2_s7m0ar_addr = &DMA2_S7M0AR;
    // dma2_s7fcr_addr = &DMA2_S7FCR;

    setup_pixbufs();

    my_ILI.begin();

    memset((void *)0x20000000, 0x11, 128 * 1024);
}

static void alloc_tile(tile *tp, size_t y, size_t h)
{
    pixbuf *pix = NULL;
    while (!pix) {
        for (size_t i = 0; i < PIXBUF_COUNT && !pix; i++) {
            WITH_INTERRUPTS_MASKED {
                if (pixbufs[i].state == PS_CLEARED) {
                    pix = &pixbufs[i];
                    pix->state = PS_DRAWING;
                }
            }
        }
    }
    tp->pixels = (typeof tp->pixels)pix->base;
    tp->height = h;
    tp->y      = y;
    tp->pix    = pix;
}

static void draw_tile(tile *tp)
{
    size_t y0 = MAX((320 - 240) / 2u, tp->y);
    size_t y1 = MIN((320 + 240) / 2u, tp->y + tp->height);
        
    for (size_t y = y0; y < y1; y++) {
        size_t x = y - (320 - 240) / 2;
        tp->pixels[y - tp->y][x] = 0x0000;
    }
}

static void draw_frame(void)
{
    tile one_tile;

    for (size_t y = 0; y < SCREEN_HEIGHT; y += one_tile.height) {
        size_t h = MIN(TILE_MAX_HEIGHT, SCREEN_HEIGHT - y);
        //XXX
        // size_t h = SCREEN_HEIGHT - y;
        // if (h > TILE_MAX_HEIGHT)
        //     h = TILE_MAX_HEIGHT;
        alloc_tile(&one_tile, y, h);
        draw_tile(&one_tile);
        send_tile(&one_tile);
    }
}

static void run(void)
{
    int frame_counter = 0;
    uint32_t next_time = 1000;

    // tile bigbuf;
    // bigbuf.pixels   = (typeof bigbuf.pixels)0x20000000;
    // bigbuf.height = 273;
    // bigbuf.y      = 0;

    while (1) {
#if 0
        clear_tile(&bigbuf, 0x07ff);
        draw_tile(&bigbuf);
        send_tile(&bigbuf);
#else
        draw_frame();
#endif
        frame_counter++;
        if (system_millis >= next_time) {
            fps = frame_counter;
            frame_counter = 0;
            next_time += 1000;
        }
    }
}

int main(void)
{
    setup();
    run();
}
