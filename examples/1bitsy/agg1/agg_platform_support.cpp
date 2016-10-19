#include "platform/agg_platform_support.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "ILI9341.h"

#define WINDOW_WIDTH 240
#define WINDOW_HEIGHT 320

#define STATIC_BUFFER_WIDTH 240
#define STATIC_BUFFER_HEIGHT (320/2)
#define STATIC_BUFFER_PIX_BYTES 2

ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

// static unsigned char static_buffer[STATIC_BUFFER_WIDTH *
//                                    STATIC_BUFFER_HEIGHT *
//                                    STATIC_BUFFER_PIX_BYTES];
static unsigned char *static_buffer = (unsigned char *)0x20000000;

namespace agg {

    platform_support::platform_support(pix_format_e format, bool flip_y)
        : m_format(format),
          m_bpp(16),
          m_wait_mode(false),
          m_flip_y(flip_y)
    {
        switch (m_format) {

        case pix_format_rgb565:
            m_bpp = 16;
            break;

        default:
            assert(false);
        }
    }

    platform_support::~platform_support()
    {}

    void platform_support::caption(const char *caption)
    {
        strlcpy(m_caption, caption, sizeof m_caption);
    }

    bool
    platform_support::init(unsigned width, unsigned height, unsigned flags)
    {
        assert(width == WINDOW_WIDTH);
        assert(height == STATIC_BUFFER_HEIGHT);
        assert(flags == 0);
        m_rbuf_window.attach(static_buffer,
                             STATIC_BUFFER_WIDTH,
                             STATIC_BUFFER_HEIGHT,
                             STATIC_BUFFER_WIDTH * STATIC_BUFFER_PIX_BYTES);
        my_ILI.begin();
        return true;
    }

    int platform_support::run()
    {
        while (true) {
            on_draw();
            my_ILI.writeRect(0, 0,
                             STATIC_BUFFER_WIDTH, STATIC_BUFFER_HEIGHT,
                             (const uint16_t *)static_buffer);
            my_ILI.writeRect(0, 320/2,
                             STATIC_BUFFER_WIDTH, STATIC_BUFFER_HEIGHT,
                             (const uint16_t *)static_buffer);
            if (!m_wait_mode)
                on_idle();
        }
    }

    void platform_support::on_init()
    {}

    void platform_support::on_resize(int sx, int sy)
    {}

    void platform_support::on_idle()
    {}

    void platform_support::on_mouse_move(int x, int y, unsigned flags)
    {}

    void platform_support::on_mouse_button_down(int x, int y, unsigned flags)
    {}

    void platform_support::on_mouse_button_up(int x, int y, unsigned flags)
    {}

    void platform_support::on_key(int x, int y, unsigned key, unsigned flags)
    {}

    void platform_support::on_ctrl_change()
    {}

    void platform_support::on_draw()
    {}

    void platform_support::on_post_draw(void* raw_handler)
    {}

    void platform_support::start_timer()
    {}

    double platform_support::elapsed_time() const
    {
        return 1.0;
    }

    void platform_support::message(const char *)
    {}

    void platform_support::update_window()
    {}

} // end namespace agg

extern "C" void abort()
{
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);
    while (true) {
        gpio_set(GPIOA, GPIO8);
        for (int i = 0; i < 100000; i++)
            __asm volatile ("nop");
        gpio_clear(GPIOA, GPIO8);
        for (int i = 0; i < 100000; i++)
            __asm volatile ("nop");
    }
}

extern "C" void *_sbrk(int incr)
{
#undef USE_CCM                 // stm32 core-coupled memory
#ifdef USE_CCM
    static uintptr_t ptr = 0x10000000;
    static const uintptr_t top_of_RAM = 0x10010000;
    if (ptr + incr > top_of_RAM) {
        errno = ENOMEM;
        return (void *)-1;
    }
    void *ret = (void *)ptr;
    ptr += incr;
    return ret;
#else
    extern void *end;
    static uintptr_t ptr = (uintptr_t)&end;
    static const uintptr_t top_of_RAM = 0x20020000;
    if (ptr + incr > top_of_RAM) {
        errno = ENOMEM;
        return (void *)-1;
    }
    void *ret = (void *)ptr;
    ptr += incr;
    return ret;
#endif
}

extern "C" int _write_r()
{
    return 0;
}

extern "C" void _close()
{
    assert(false);
}

extern "C" void _fstat()
{
    assert(false);
}

extern "C" void _isatty()
{
    assert(false);
}

extern "C" void _lseek()
{
    assert(false);
}

extern "C" void _read()
{
    assert(false);
}
