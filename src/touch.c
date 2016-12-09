#include <touch.h>

#include <assert.h>

#include <libopencm3/stm32/i2c.h>

#include <i2c.h>
#include <lcd.h>

#define FT6206_ADDRESS          0x70
#define FT6206_THRESHOLD          40
#define FT6206_VENDOR_ID          17
#define FT6206_CIPHER             16

#define FT6206_REG_P1_XH        0x03
#define FT6206_REG_P1_XL        0x04
#define FT6206_REG_P1_YH        0x05
#define FT6206_REG_P1_YL        0x06
#define FT6206_REG_P2_XH        0x09
#define FT6206_REG_P2_XL        0x0A
#define FT6206_REG_P2_YH        0x0B
#define FT6206_REG_P2_YL        0x0C
#define FT6206_REG_TD_STATUS    0x02
#define FT6206_REG_CIPHER       0xA3
#define FT6206_REG_FOCALTECH_ID 0xA8


static const i2c_channel ft6206_channel = {
    .i_base_address = I2C1,
    .i_is_master    = true,
    .i_stop         = true,
    .i_address      = FT6206_ADDRESS,
};

static uint8_t ft6206_read_register(uint8_t reg)
{
    const uint8_t out[1] = { reg };
    uint8_t in[1] = { 0xFF };
    i2c_transmit(&ft6206_channel, out, 1);
    i2c_receive(&ft6206_channel, in, 1);
    return in[0];
}

// static void ft6202_write_register(uint8_t reg, uint8_t value)
// {
//     const uint8_t out[2] = { reg, value };
//     i2c_transmit(&ft6206_channel, out, 2);
// }

void touch_init(void)
{
    static const i2c_config config = {
        .i_base_address    = I2C1,
        .i_own_address     = 32,
        .i_pins = {
            {                   // SCL: pin PB6, AF4
                .gp_port   = GPIOB,
                .gp_pin    = GPIO6,
                .gp_mode   = GPIO_MODE_AF,
                .gp_pupd   = GPIO_PUPD_NONE,
                .gp_af     = GPIO_AF4,
                .gp_ospeed = GPIO_OSPEED_50MHZ,
                .gp_otype  = GPIO_OTYPE_OD,
                .gp_level  = 1,

            },
            {                   // SDA: pin PB7, AF4
                .gp_port   = GPIOB,
                .gp_pin    = GPIO7,
                .gp_mode   = GPIO_MODE_AF,
                .gp_pupd   = GPIO_PUPD_NONE,
                .gp_af     = GPIO_AF4,
                .gp_ospeed = GPIO_OSPEED_50MHZ,
                .gp_otype  = GPIO_OTYPE_OD,
                .gp_level  = 1,
            },
        },
    };
    init_i2c(&config);

    // uint8_t vendor_id = ft6206_read_register(FT6206_REG_FOCALTECH_ID);
    // // assert(vendor_id == FT6206_VENDOR_ID);
    // uint8_t cipher = ft6206_read_register(FT6206_REG_CIPHER);
    // assert(cipher == FT6206_CIPHER);
}

size_t touch_count(void)
{
    return ft6206_read_register(FT6206_REG_TD_STATUS) & 0x0F;
}

gfx_ipoint touch_point(size_t index)
{
    uint8_t first_reg;
    switch (index) {

    case 0:
        first_reg = FT6206_REG_P1_XH;
        break;

    case 1:
        first_reg = FT6206_REG_P2_XH;
        break;

    default:
        assert(false);
    }

    const uint8_t out[2] = { first_reg, 4 };
    uint8_t in[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    i2c_transmit(&ft6206_channel, out, 2);
    i2c_receive(&ft6206_channel, in, 4);
    int raw_x = (in[0] << 8 & 0x0F00) | in[1];
    int raw_y = (in[2] << 8 & 0x0F00) | in[3];
    return (gfx_ipoint) {
        .x = LCD_WIDTH  - raw_x - 1,
        .y = LCD_HEIGHT - raw_y - 1,
    };
}
