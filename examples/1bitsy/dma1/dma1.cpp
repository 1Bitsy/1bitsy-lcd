// Feature switches
#define BG_DEBUG
#define CLEAR_DMA
#define VIDEO_DMA

// C/POSIX headers
#include <assert.h>
#include <string.h>

// libopencm3 headers
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/timer.h>

// application headers
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
#define TILE_COUNT 2
#define TILE_MAX_SIZE_BYTES (RAM_SIZE / TILE_COUNT)

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

#define TILE_WIDTH SCREEN_WIDTH
#define TILE_MAX_HEIGHT (TILE_MAX_SIZE_BYTES / (TILE_WIDTH * sizeof (uint16_t)))

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
    if (frame == 3 * 256)
        frame = 0;
    return color;
}

#endif

typedef enum tile_state {
    TS_CLEARED,
    TS_DRAWING,
    TS_SEND_WAIT,
    TS_SENDING,
    TS_CLEAR_WAIT,
    TS_CLEARING,
} tile_state;

typedef struct tile {
    uint16_t (*pixels)[TILE_WIDTH];
    size_t     y;
    size_t     height;
    tile_state state;
} tile;

static tile tiles[TILE_COUNT];

static inline size_t tile_size_bytes(tile *tp)
{
    return tp->height * sizeof *tp->pixels;
}

volatile int fps;

#ifdef CLEAR_DMA

static volatile bool clear_dma_busy;

static void start_clear_dma(tile *tp)
{
    size_t size = TILE_MAX_SIZE_BYTES;
    uintptr_t base = (uintptr_t)tp->pixels;
    assert(0x20000000 <= base);
    assert(base + size <= 0x20020000);
    assert(size >= 16);
    assert(!(size & 0xF));      // multiple of 16 bytes

    // Fill the first 16 bytes with the pixel.  Then
    // DMA will duplicate that through the buffer.
    uint32_t *p = (uint32_t *)tp->pixels;
    const int pburst = 4;

#ifdef BG_DEBUG    
    uint32_t color = calc_bg_color();
    uint32_t pix_twice = color << 16 | color;
#else
    uint32_t pix_twice = BG_COLOR << 16 | BG_COLOR;
#endif

    DMA2_S7CR &= ~DMA_SxCR_EN;  // XXX
    while (DMA2_S7CR & DMA_SxCR_EN) // XXX
        continue;                   // XXX

    for (int i = 0; i < pburst; i++)
        *p++ = pix_twice;

    // for (int i = 0; i < 1000; i++) {
    //     volatile uint32_t x = ((uint32_t *)tp->pixels)[i];
    // }

    DMA2_S7CR &= ~DMA_SxCR_EN;
    while (DMA2_S7CR & DMA_SxCR_EN)
        continue;

    DMA2_S7PAR  = (void *)tp->pixels;
    DMA2_S7M0AR = p;
    DMA2_S7NDTR = (size - 16) / 4;
    DMA2_S7FCR  = (DMA_SxFCR_FEIE          |
                   DMA_SxFCR_DMDIS         |
                   DMA_SxFCR_FTH_4_4_FULL);
    DMA2_S7CR = (DMA_SxCR_CHSEL_0          |
                   DMA_SxCR_MBURST_INCR4   |
                   DMA_SxCR_PBURST_INCR4   |
                  !DMA_SxCR_DBM            |
                   DMA_SxCR_PL_LOW         |
                  !DMA_SxCR_PINCOS         |
                   DMA_SxCR_MSIZE_32BIT    |
                   DMA_SxCR_PSIZE_32BIT    |
                   DMA_SxCR_MINC           |
                  !DMA_SxCR_PINC           |
                  !DMA_SxCR_CIRC           |
                   DMA_SxCR_DIR_MEM_TO_MEM |
                  !DMA_SxCR_PFCTRL         |
                   DMA_SxCR_TCIE           |
                  !DMA_SxCR_HTIE           |
                   DMA_SxCR_TEIE           |
                   DMA_SxCR_DMEIE          |
                   DMA_SxCR_EN);
}

void dma2_stream7_isr(void)
{
    const uint32_t ERR_BITS = DMA_HISR_TEIF7 | DMA_HISR_DMEIF7 | DMA_HISR_FEIF7;
    const uint32_t CLEAR_BITS = DMA_HISR_TCIF7 | DMA_HISR_HTIF7 | ERR_BITS;
    uint32_t dma2_hisr = DMA2_HISR;
    DMA2_HIFCR = dma2_hisr & CLEAR_BITS;
    assert((dma2_hisr & ERR_BITS) == 0);

    assert(dma2_hisr & DMA_HISR_TCIF7);

    DMA2_S7CR  = 0;
    DMA2_HIFCR = CLEAR_BITS;

    clear_dma_busy = false;
    for (size_t i = 0; i < TILE_COUNT; i++) {
        tile *tp = &tiles[i];
        if (tp->state == TS_CLEARING) {
            tp->state = TS_CLEARED;
        } else if (tp->state == TS_CLEAR_WAIT && !clear_dma_busy) {
            tp->state = TS_CLEARING;
            clear_dma_busy = true;
            start_clear_dma(tp);
        }
    }
}

static void clear_tile(tile *tp)
{
    bool busy;
    WITH_INTERRUPTS_MASKED {
        busy = clear_dma_busy;
        if (busy) {
            tp->state = TS_CLEAR_WAIT;
        } else {
            clear_dma_busy = true;
            tp->state = TS_CLEARING;
        }
    }
    if (!busy)
        start_clear_dma(tp);
}

static void setup_clear_dma(void)
{
    rcc_periph_clock_enable(RCC_DMA2);
    nvic_enable_irq(NVIC_DMA2_STREAM7_IRQ);
}

#else /* !CLEAR_DMA */

static void clear_tile(tile *tp)
{
#ifdef BG_DEBUG    
    uint32_t color = calc_bg_color();
    uint32_t pix_twice = color << 16 | color;
#else
    uint32_t pix_twice = BG_COLOR << 16 | BG_COLOR;
#endif

    uint32_t *p = (uint32_t *)tp->pixels;
    size_t n = TILE_MAX_SIZE_BYTES / sizeof *p;
    for (size_t i = 0; i < n; i++)
        *p++ = pix_twice;
    tp->state = TS_CLEARED;
}

static void setup_clear_dma(void)
{}

#endif /* CLEAR_DMA */

#ifdef VIDEO_DMA

#define LCD_CSX_PORT   GPIOC
#define LCD_CSX_PIN    GPIO3
#define LCD_RESX_PORT  GPIOC
#define LCD_RESX_PIN   GPIO2
#define LCD_DCX_PORT   GPIOC
#define LCD_DCX_PIN    GPIO6
#define LCD_WRX_PORT   GPIOB
#define LCD_WRX_PIN    GPIO1
#define LCD_RDX_PORT   GPIOB
#define LCD_RDX_PIN    GPIO0
#define LCD_DATA_PORT  GPIOB
#define LCD_DATA_PINS ((GPIO15 << 1) - GPIO8)


#define ILI9341_NOP     0x00
#define ILI9341_SWRESET 0x01
#define ILI9341_RDDID   0x04
#define ILI9341_RDDST   0x09

#define ILI9341_SLPIN   0x10
#define ILI9341_SLPOUT  0x11
#define ILI9341_PTLON   0x12
#define ILI9341_NORON   0x13

#define ILI9341_RDMODE  0x0A
#define ILI9341_RDMADCTL  0x0B
#define ILI9341_RDPIXFMT  0x0C
#define ILI9341_RDIMGFMT  0x0D
#define ILI9341_RDSELFDIAG  0x0F

#define ILI9341_INVOFF  0x20
#define ILI9341_INVON   0x21
#define ILI9341_GAMMASET 0x26
#define ILI9341_DISPOFF 0x28
#define ILI9341_DISPON  0x29

#define ILI9341_CASET   0x2A
#define ILI9341_PASET   0x2B
#define ILI9341_RAMWR   0x2C
#define ILI9341_RAMRD   0x2E

#define ILI9341_PTLAR    0x30
#define ILI9341_MADCTL   0x36
#define ILI9341_VSCRSADD 0x37
#define ILI9341_PIXFMT   0x3A

#define ILI9341_FRMCTR1 0xB1
#define ILI9341_FRMCTR2 0xB2
#define ILI9341_FRMCTR3 0xB3
#define ILI9341_INVCTR  0xB4
#define ILI9341_DFUNCTR 0xB6

#define ILI9341_PWCTR1  0xC0
#define ILI9341_PWCTR2  0xC1
#define ILI9341_PWCTR3  0xC2
#define ILI9341_PWCTR4  0xC3
#define ILI9341_PWCTR5  0xC4
#define ILI9341_VMCTR1  0xC5
#define ILI9341_VMCTR2  0xC7

#define ILI9341_RDID1   0xDA
#define ILI9341_RDID2   0xDB
#define ILI9341_RDID3   0xDC
#define ILI9341_RDID4   0xDD

#define ILI9341_GMCTRP1 0xE0
#define ILI9341_GMCTRN1 0xE1
// #define ILI9341_PWCTR6  0xFC
#define ILI9341_IFCTL   0xF6

static const uint8_t init_commands[] = {
    4,  0xEF, 0x03, 0x80, 0x02,
    4,  0xCF, 0x00, 0XC1, 0X30,
    5,  0xED, 0x64, 0x03, 0X12, 0X81,
    4,  0xE8, 0x85, 0x00, 0x78,
    6,  0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
    2,  0xF7, 0x20,
    3,  0xEA, 0x00, 0x00,
    2,  ILI9341_PWCTR1, 0x23, // Power control
    2,  ILI9341_PWCTR2, 0x10, // Power control
    3,  ILI9341_VMCTR1, 0x3e, 0x28, // VCM control
    2,  ILI9341_VMCTR2, 0x86, // VCM control2

    4,  ILI9341_IFCTL, 0x00, 0x00, 0x20,

    2,  ILI9341_MADCTL, 0x48, // Memory Access Control
    2,  ILI9341_PIXFMT, 0x55,
    3,  ILI9341_FRMCTR1, 0x00, 0x18,
    4,  ILI9341_DFUNCTR, 0x08, 0x82, 0x27, // Display Function Control
    2,  0xF2, 0x00, // Gamma Function Disable
    2,  ILI9341_GAMMASET, 0x01, // Gamma curve selected
    16, ILI9341_GMCTRP1, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
                         0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03,
                         0x0E, 0x09, 0x00, // Set Gamma
    16, ILI9341_GMCTRN1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
                         0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C,
                         0x31, 0x36, 0x0F, // Set Gamma
    0
};

static volatile bool video_dma_busy;

static void bang8(uint8_t data, bool cmd, bool done)
{
    if (cmd) {
        gpio_clear(LCD_CSX_PORT, LCD_CSX_PIN);
        gpio_clear(LCD_DCX_PORT, LCD_DCX_PIN);
    }
    gpio_clear(LCD_WRX_PORT, LCD_WRX_PIN);
#if LCD_DATA_PINS == 0x00FF
    ((volatile uint8_t *)&GPIO_ODR(LCD_DATA_PORT))[0] = data;
#elif LCD_DATA_PINS == 0xFF00
    ((volatile uint8_t *)&GPIO_ODR(LCD_DATA_PORT))[1] = data;
#else
    #error "data pins must be byte aligned."
#endif
    gpio_set(LCD_WRX_PORT, LCD_WRX_PIN);
    if (cmd)
        gpio_set(LCD_DCX_PORT, LCD_DCX_PIN);
    if (done)
        gpio_set(LCD_CSX_PORT, LCD_CSX_PIN);
}

static void bang16(uint16_t data, bool done)
{
    bang8(data >> 8, false, false);
    bang8(data & 0xFF, false, done);
}

static void start_video_dma(tile *tp)
{
    // Configure DMA.
    {
        DMA2_S1CR &= ~DMA_SxCR_EN;
        while (DMA2_S1CR & DMA_SxCR_EN)
            continue;

#if LCD_DATA_PINS == 0x00FF
        DMA2_S1PAR  = (uint8_t *)&GPIO_ODR(LCD_DATA_PORT);
#elif LCD_DATA_PINS == 0xFF00
        DMA2_S1PAR  = (uint8_t *)&GPIO_ODR(LCD_DATA_PORT) + 1;
#else
        #error        "data pins must be byte aligned."
#endif
        DMA2_S1M0AR = tp->pixels;
        DMA2_S1NDTR = tile_size_bytes(tp);
        DMA2_S1FCR  = (DMA_SxFCR_FEIE                 |
                       DMA_SxFCR_DMDIS                |
                       DMA_SxFCR_FTH_4_4_FULL);
        DMA2_S1CR   = (DMA_SxCR_CHSEL_7               |
                       DMA_SxCR_MBURST_INCR4          |
                       DMA_SxCR_PBURST_SINGLE         |
                      !DMA_SxCR_CT                    |
                      !DMA_SxCR_DBM                   |
                       DMA_SxCR_PL_VERY_HIGH          |
                      !DMA_SxCR_PINCOS                |
                       DMA_SxCR_MSIZE_32BIT           |
                       DMA_SxCR_PSIZE_8BIT            |
                       DMA_SxCR_MINC                  |
                      !DMA_SxCR_PINC                  |
                       DMA_SxCR_CIRC                  |
                       DMA_SxCR_DIR_MEM_TO_PERIPHERAL |
                      !DMA_SxCR_PFCTRL                |
                       DMA_SxCR_TCIE                  |
                      !DMA_SxCR_HTIE                  |
                       DMA_SxCR_TEIE                  |
                       DMA_SxCR_DMEIE                 |
                       DMA_SxCR_EN);
    }

    // Configure Timer.
    {
        TIM8_CR1    = 0;
        TIM8_CR1    = (TIM_CR1_CKD_CK_INT             |
                      !TIM_CR1_ARPE                   |
                       TIM_CR1_CMS_EDGE               |
                       TIM_CR1_DIR_UP                 |
                      !TIM_CR1_OPM                    |
                     ! TIM_CR1_URS                    | // XXX
                      !TIM_CR1_UDIS                   |
                      !TIM_CR1_CEN);
        TIM8_CR2    = (
                      !TIM_CR2_TI1S                   |
                       TIM_CR2_MMS_RESET              |
                       TIM_CR2_CCDS);
        TIM8_SMCR   = 0;
        TIM8_DIER   = (
                      !TIM_DIER_TDE                   |
                      !TIM_DIER_CC4DE                 |
                      !TIM_DIER_CC3DE                 |
                      !TIM_DIER_CC2DE                 |
                      !TIM_DIER_CC1DE                 |
                       TIM_DIER_UDE                   |
                      !TIM_DIER_TIE                   |
                      !TIM_DIER_CC4IE                 |
                      !TIM_DIER_CC3IE                 |
                      !TIM_DIER_CC2IE                 |
                      !TIM_DIER_CC1IE                 |
                      !TIM_DIER_UIE);
        TIM8_SR     = 0;
        TIM8_EGR    = TIM_EGR_UG;
        TIM8_CCMR1  = 0;
        TIM8_CCMR2  = (TIM_CCMR2_CC3S_OUT             |
                       TIM_CCMR2_OC3M_PWM2);
        TIM8_CCER   = TIM_CCER_CC3NE; // XXX
        TIM8_CCER   = (
                      !TIM_CCER_CC4P                  |
                      !TIM_CCER_CC4E                  |
                      !TIM_CCER_CC3NP                 |
                       TIM_CCER_CC3NE                 |
                      !TIM_CCER_CC3P                  |
                      !TIM_CCER_CC3E                  |
                      !TIM_CCER_CC2NP                 |
                      !TIM_CCER_CC2NE                 |
                      !TIM_CCER_CC2P                  |
                      !TIM_CCER_CC2E                  |
                      !TIM_CCER_CC1NP                 |
                      !TIM_CCER_CC1NE                 |
                      !TIM_CCER_CC1P                  |
                      !TIM_CCER_CC1E);
        TIM8_CNT    = 0;
        TIM8_PSC    = 0;
        TIM8_ARR    = 34/2;
        // TIM8_CCR3   = 17/2;
        TIM8_CCR3   = 12;
        TIM8_BDTR   = TIM_BDTR_MOE | TIM_BDTR_OSSR;
        // TIM8_DCR    = 0;
        // TIM8_DMAR   = 0;
    }

    // Bit-bang the ILI9341 RAM address range.
    bang8(ILI9341_CASET, true, false);
    bang16(0, false);
    bang16(TILE_WIDTH - 1, false);

    bang8(ILI9341_PASET, true, false);
    bang16(tp->y, false);
    bang16(tp->y + tp->height - 1, false);

    // Bit-bang the command word.
    bang8(ILI9341_RAMWR, true, false);

    // Switch the LCD_WRX pin to timer control.
    gpio_set_af(LCD_WRX_PORT, GPIO_AF3, LCD_WRX_PIN);
    gpio_mode_setup(LCD_WRX_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, LCD_WRX_PIN);

    // Start the timer.
    TIM8_CR1 |= TIM_CR1_CEN;
}

void dma2_stream1_isr(void)
{
    const uint32_t ERR_BITS = DMA_LISR_TEIF1 | DMA_LISR_DMEIF1 | DMA_LISR_FEIF1;
    const uint32_t CLEAR_BITS = DMA_LISR_TCIF1 | DMA_LISR_HTIF1 | ERR_BITS;
    uint32_t dma2_lisr = DMA2_LISR;
    DMA2_LIFCR = dma2_lisr & CLEAR_BITS;
    assert((dma2_lisr & ERR_BITS) == 0);

    if (dma2_lisr & DMA_LISR_TCIF1) {
        // Transfer done.
        //  - Deselect ILI9341.
        //  - Switch LCD_WRX and LCD_RDX pins back to GPIO mode.
        //  - Stop the timer.
        //  - Stop the DMA.
        gpio_set(LCD_CSX_PORT, LCD_CSX_PIN);
        gpio_mode_setup(LCD_WRX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_WRX_PIN);
        gpio_mode_setup(LCD_RDX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_RDX_PIN);
        TIM8_CR1   = 0;
        DMA2_S1CR  = 0;
        DMA2_LIFCR = CLEAR_BITS;

        video_dma_busy = false;
        for (size_t i = 0; i < TILE_COUNT; i++) {
            tile *tp = &tiles[i];
            if (tp->state == TS_SENDING) {
                clear_tile(tp);
            } else if (tp->state == TS_SEND_WAIT && !video_dma_busy) {
                tp->state = TS_SENDING;
                video_dma_busy = true;
                start_video_dma(tp);
            }
        }
    }
}

static void send_tile(tile *tp)
{
    bool busy;
    WITH_INTERRUPTS_MASKED {
        busy = video_dma_busy;
        if (busy) {
            tp->state = TS_SEND_WAIT;
        } else {
            video_dma_busy = true;
            tp->state = TS_SENDING;
        }
    }
    if (!busy)
        start_video_dma(tp);
}

volatile uint32_t *tim8_cr1_addr;
volatile uint32_t *tim8_cnt_addr;
volatile uint32_t *tim8_arr_addr;
volatile uint32_t *tim8_psc_addr;
volatile uint32_t *dma2_lisr_addr;
volatile uint32_t *dma2_s1cr_addr;
volatile uint32_t *dma2_s1ndtr_addr;
volatile void    **dma2_s1par_addr;
volatile void    **dma2_s1m0ar_addr;
volatile uint32_t *dma2_s1fcr_addr;
volatile uint32_t *gpiob_odr_addr;

static void setup_video_dma(void)
{
    tim8_cr1_addr    = &TIM8_CR1;
    tim8_cnt_addr    = &TIM8_CNT;
    tim8_arr_addr    = &TIM8_ARR;
    tim8_psc_addr    = &TIM8_PSC;
    dma2_lisr_addr   = &DMA2_LISR;
    dma2_s1cr_addr   = &DMA2_S1CR;
    dma2_s1ndtr_addr = &DMA2_S1NDTR;
    dma2_s1par_addr  = &DMA2_S1PAR;
    dma2_s1m0ar_addr = &DMA2_S1M0AR;
    dma2_s1fcr_addr  = &DMA2_S1FCR;
    gpiob_odr_addr   = &GPIOB_ODR;

    // RCC
    rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_GPIOC);

    // GPIO
    {
        // CSX (chip select)
        gpio_mode_setup(LCD_CSX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_CSX_PIN);
        gpio_set(LCD_CSX_PORT, LCD_CSX_PIN);

        // RESX (reset)
        gpio_mode_setup(LCD_RESX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_RESX_PIN);

        // DCX (data/command)
        gpio_mode_setup(LCD_DCX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_DCX_PIN);
        gpio_set(LCD_DCX_PORT, LCD_DCX_PIN);

        // WRX (World Rallycross)
        gpio_mode_setup(LCD_WRX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_WRX_PIN);
        gpio_set(LCD_WRX_PORT, LCD_WRX_PIN);

        // RDX (read enable)
        gpio_mode_setup(LCD_RDX_PORT,
                        GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                        LCD_RDX_PIN);
        gpio_set(LCD_RDX_PORT, LCD_RDX_PIN);

        // Do the data pins together.
        gpio_mode_setup(LCD_DATA_PORT,
                        GPIO_MODE_OUTPUT,
                        GPIO_PUPD_NONE,
                        LCD_DATA_PINS);
    }
        
    // TIMER
    rcc_periph_clock_enable(RCC_TIM8);

    // DMA: DMA controller 2, stream 1, channel 7.
    rcc_periph_clock_enable(RCC_DMA2);
    nvic_enable_irq(NVIC_DMA2_STREAM1_IRQ);

    // Initialize ILI9341.
    {
        // toggle RST low to reset
        gpio_set(LCD_RESX_PORT, LCD_RESX_PIN);
        delay_msec(5);
        gpio_clear(LCD_RESX_PORT, LCD_RESX_PIN);
        delay_msec(20);
        gpio_set(LCD_RESX_PORT, LCD_RESX_PIN);
        delay_msec(150);

        // Send commands to ILI9341.
        const uint8_t *addr = init_commands;
        while (1) {
            uint8_t count = *addr++;
            if (count-- == 0)
                break;
            bang8(*addr++, true, false);
            while (count-- > 0) {
                bang8(*addr++, false, false);
            }
        }
        bang8(ILI9341_SLPOUT, true, true); // Exit sleep.

        delay_msec(120);                
        bang8(ILI9341_DISPON, true, true);
    }
}

#else

#include "ILI9341.h"

ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

static void send_tile(tile *tp)
{
    my_ILI.writeRect(0, tp->y, TILE_WIDTH, tp->height, tp->pixels[0]);
    clear_tile(tp);
}

static void setup_video_dma(void)
{
    my_ILI.begin();
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

static void setup_tiles(void)
{
    for (size_t i = 0; i < TILE_COUNT; i++) {
        tile *tp = tiles + i;
        tp->pixels = (typeof tp->pixels)(0x20000000 + i * TILE_MAX_SIZE_BYTES);
        clear_tile(tp);
    }
}

static void setup(void)
{
    rcc_clock_setup_hse_3v3(&MY_CLOCK);

    setup_systick(MY_CLOCK.ahb_frequency);
    setup_heartbeat();

    setup_video_dma();
    setup_clear_dma();

    setup_tiles();
}

static tile *alloc_tile(size_t y, size_t h)
{
    tile *tp = NULL;
    while (!tp) {
        for (size_t i = 0; i < TILE_COUNT && !tp; i++) {
            WITH_INTERRUPTS_MASKED {
                if (tiles[i].state == TS_CLEARED) {
                    tp = &tiles[i];
                    tp->state = TS_DRAWING;
                }
            }
        }
    }
    tp->height = h;
    tp->y      = y;
    return tp;
}

static void draw_tile(tile *tp)
{
    static size_t y0 = 0;
    static int inc = +1;
    size_t y1 = y0 + 240;
    for (size_t y = tp->y; y < tp->y + tp->height; y++) {
        if (y >= y0 && y < y1) {
            tp->pixels[y - tp->y][y - y0] = 0x0000;
            tp->pixels[y - tp->y][239 - y + y0] = 0x0000;
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
}

static void draw_frame(void)
{
    size_t h;

    for (size_t y = 0; y < SCREEN_HEIGHT; y += h) {
        h = MIN(TILE_MAX_HEIGHT, SCREEN_HEIGHT - y);
        tile *tp = alloc_tile(y, h);
        draw_tile(tp);
        send_tile(tp);
    }
}

static void run(void)
{
    int frame_counter = 0;
    uint32_t next_time = 1000;

    while (1) {
        draw_frame();
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
