#include "ILI9341.h"
#include "systick.h"
#include "platform/agg_platform_support.h"

#define MY_CLOCK (rcc_hse_25mhz_3v3[RCC_CLOCK_3V3_168MHZ])

#define LED_PORT GPIOA
#define LED_PIN GPIO8
#define LED_RCC_PORT RCC_GPIOA

extern int agg_main(int argc, char *argv[]);

// ILI9341_t3 my_ILI(0, 0, 0, 0, 0, 0);

static void heartbeat(uint32_t msec_time)
{
    if (msec_time & 0x400)
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

    // my_ILI.begin();
}

static void run()
{
    char agg[] = "agg";
    char *argv[] = {
        agg,
        NULL
    };
    (void)agg_main(1, argv);
}

int main(void)
{
    setup();
    run();
}
