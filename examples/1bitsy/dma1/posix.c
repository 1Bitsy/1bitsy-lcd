#include <assert.h>
#include <errno.h>
#include <stdbool.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

extern void abort(void);
void abort(void)
{
    cm_disable_interrupts();
    rcc_periph_clock_enable(RCC_GPIOA);
    gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO8);
    while (true) {
        gpio_set(GPIOA, GPIO8);
        for (int i = 0; i < 1000000; i++)
            __asm volatile ("nop");
        gpio_clear(GPIOA, GPIO8);
        for (int i = 0; i < 1000000; i++)
            __asm volatile ("nop");
    }
}

extern void *_sbrk(int incr);
void *_sbrk(int incr)
{
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
}

extern int _write_r(void);
int _write_r(void)
{
    return 0;
}

extern void _close(void);
void _close(void)
{
    assert(false);
}

extern void _fstat(void);
void _fstat(void)
{
    assert(false);
}

extern void _isatty(void);
void _isatty(void)
{
    assert(false);
}

extern void _lseek(void);
void _lseek(void)
{
    assert(false);
}

extern void _read(void);
void _read(void)
{
    assert(false);
}
