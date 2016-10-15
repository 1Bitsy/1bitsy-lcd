#include <assert.h>
#include <string.h>

#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>

#include <libopencm3/stm32/rcc.h>

#include "ILI9341.h"
#include "systick.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define LED_PORT GPIOA
#define LED_PIN GPIO8
#define LED_RCC_PORT RCC_GPIOA

#define RAM_SIZE (128 << 10)    // 128 Kbytes
#define PIXBUF_COUNT 2

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define TILE_WIDTH SCREEN_WIDTH
#define TILE_MAX_HEIGHT (RAM_SIZE / PIXBUF_COUNT /              \
                         TILE_WIDTH * sizeof (uint16_t))

typedef struct tile {
    uint16_t (*base)[TILE_WIDTH];
    size_t    height;
    size_t    y;
} tile;

static inline size_t tile_size_bytes(tile *tp)
{
    assert(sizeof *tp->base == 480); // XXX
    return tp->height * sizeof *tp->base;
}

ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

volatile int fps;
static volatile uint32_t dma_intr_count;

volatile uint32_t *dma2_hisr_addr;
volatile uint32_t *dma2_s7cr_addr;
volatile uint32_t *dma2_s7ndtr_addr;
volatile void    **dma2_s7par_addr;
volatile void    **dma2_s7m0ar_addr;
volatile uint32_t *dma2_s7fcr_addr;

uint32_t dma2_hisr;
uint32_t dma2_s7cr;
uint32_t dma2_s7ndtr;
volatile void *dma2_s7par;
volatile void *dma2_s7m0ar;
uint32_t dma2_s7fcr;

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

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);

    setup_heartbeat();

    rcc_periph_clock_enable(RCC_DMA2);
    nvic_enable_irq(NVIC_DMA2_STREAM7_IRQ);
    dma2_hisr_addr = &DMA2_HISR;
    dma2_s7cr_addr = &DMA2_S7CR;
    dma2_s7ndtr_addr = &DMA2_S7NDTR;
    dma2_s7par_addr = &DMA2_S7PAR;
    dma2_s7m0ar_addr = &DMA2_S7M0AR;
    dma2_s7fcr_addr = &DMA2_S7FCR;

    my_ILI.begin();

    memset((void *)0x20000000, 0x11, 128 * 1024);
}

#define CLEAR_DMA
#ifdef CLEAR_DMA

static void clear_tile(tile *tp, uint16_t color)
{
    size_t size = tile_size_bytes(tp);
    uintptr_t base = (uintptr_t)tp->base;
    assert(0x20000000 <= base);
    assert(base + size <= 0x20020000);
    assert(size >= 16);
    assert(!(size & 0xF));      // multiple of 16 bytes

    // Fill the first 16 bytes with the pixel.  Then
    // DMA will duplicate that through the buffer.
    uint32_t *p = (uint32_t *)tp->base;
    const int pburst = 4;
    uint32_t pix_twice = (uint32_t)color << 16 | color;
    for (int i = 0; i < pburst; i++)
        *p++ = pix_twice;

    uint32_t ic = dma_intr_count;

    DMA2_S7CR &= ~DMA_SxCR_EN;
    while (DMA2_S7CR & DMA_SxCR_EN)
        continue;

    DMA2_S7PAR  = tp->base;
    DMA2_S7M0AR = p;
    DMA2_S7NDTR = size - 16;
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

    while (dma_intr_count == ic)
        continue;

    dma2_s7cr   = DMA2_S7CR;
    dma2_s7ndtr = DMA2_S7NDTR;
    dma2_s7par  = DMA2_S7PAR;
    dma2_s7m0ar = DMA2_S7M0AR;
    dma2_s7fcr  = DMA2_S7FCR;
}

void dma2_stream7_isr(void)
{
    dma2_hisr = DMA2_HISR;
    DMA2_HIFCR = dma2_hisr & 0x0F400000;
    dma_intr_count++;
}

#else // !CLEAR_DMA

static void clear_tile(tile *tp, uint16_t color)
{
    uint32_t pix_twice = (uint32_t)color << 16 | color;
    uint32_t *p = (uint32_t *)tp->base;
    size_t n = tile_size_bytes(tp) / sizeof *p;
    for (size_t i = 0; i < n; i++)
        *p++ = pix_twice;
}

#endif // CLEAR_DMA

#undef VIDEO_DMA
#ifdef VIDEO_DMA

#error "Write me!"

#else

static void send_tile(tile *tp)
{
    my_ILI.writeRect(0, tp->y, TILE_WIDTH, tp->height, tp->base[0]);
}

#endif

// static void alloc_tile(tile *tp, size_t y, size_t h)
// {
//     assert(TILE_MAX_HEIGHT == 136); // XXX
//     assert(h <= TILE_MAX_HEIGHT);
//     // while (1) {
//     //     // if 
//     // }
// }

static void draw_tile(tile *tp)
{
    (void)tp;
}

// static void draw_frame(void)
// {
//     tile one_tile;

//     for (size_t y = 0; y < SCREEN_HEIGHT; y += one_tile.height) {
//         size_t h = SCREEN_HEIGHT - y;
//         if (h > TILE_MAX_HEIGHT)
//             h = TILE_MAX_HEIGHT;
//         alloc_tile(&one_tile, y, h);
//         clear_tile(&one_tile, 0x07ff);
//         draw_tile(&one_tile);
//         send_tile(&one_tile);
//     }
// }

static void run(void)
{
    int frame_counter = 0;
    uint32_t next_time = 1000;

    tile bigbuf;
    bigbuf.base   = (typeof bigbuf.base)0x20000000;
    bigbuf.height = 273;
    bigbuf.y      = 0;

    while (1) {
        clear_tile(&bigbuf, 0x07ff);
        draw_tile(&bigbuf);
        send_tile(&bigbuf);
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
