#include "touch.h"

#include "delay.h"
#include "iic.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include <string.h>

#define TOUCH_SCL_PORT GPIOB
#define TOUCH_SCL_PIN  GPIO_Pin_1
#define TOUCH_SDA_PORT GPIOF
#define TOUCH_SDA_PIN  GPIO_Pin_9
#define TOUCH_INT_PORT GPIOF
#define TOUCH_INT_PIN  GPIO_Pin_10
#define TOUCH_RST_PORT GPIOF
#define TOUCH_RST_PIN  GPIO_Pin_11
#define GT9147_I2C_ADDRESS 0x14U

static struct iic_desc s_touch_iic =
{
    .Scl_port = TOUCH_SCL_PORT,
    .Scl_pin = TOUCH_SCL_PIN,
    .Sda_port = TOUCH_SDA_PORT,
    .Sda_pin = TOUCH_SDA_PIN
};

#define GT9147_CTRL_REG     0x8040U
#define GT9147_CONFIG_REG   0x8047U
#define GT9147_CHECK_REG    0x80FFU
#define GT9147_PID_REG      0x8140U
#define GT9147_STATUS_REG   0x814EU
#define GT9147_POINT1_REG   0x8150U
#define GT9147_POINT2_REG   0x8158U
#define GT9147_POINT3_REG   0x8160U
#define GT9147_POINT4_REG   0x8168U
#define GT9147_POINT5_REG   0x8170U

#define TOUCH_PRES_DOWN  0x80U
#define TOUCH_CATH_PRES  0x40U
#define TOUCH_MAX_POINTS 5U

static const uint16_t s_point_registers[TOUCH_MAX_POINTS] =
{
    GT9147_POINT1_REG, GT9147_POINT2_REG, GT9147_POINT3_REG,
    GT9147_POINT4_REG, GT9147_POINT5_REG
};

/* The exact configuration table used by experiment 27. */
static const uint8_t s_gt9147_config[] =
{
    0x60,0xE0,0x01,0x20,0x03,0x05,0x35,0x00,0x02,0x08,
    0x1E,0x08,0x50,0x3C,0x0F,0x05,0x00,0x00,0xFF,0x67,
    0x50,0x00,0x00,0x18,0x1A,0x1E,0x14,0x89,0x28,0x0A,
    0x30,0x2E,0xBB,0x0A,0x03,0x00,0x00,0x02,0x33,0x1D,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x00,0x00,
    0x2A,0x1C,0x5A,0x94,0xC5,0x02,0x07,0x00,0x00,0x00,
    0xB5,0x1F,0x00,0x90,0x28,0x00,0x77,0x32,0x00,0x62,
    0x3F,0x00,0x52,0x50,0x00,0x52,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,
    0x0F,0x03,0x06,0x10,0x42,0xF8,0x0F,0x14,0x00,0x00,
    0x00,0x00,0x1A,0x18,0x16,0x14,0x12,0x10,0x0E,0x0C,
    0x0A,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x29,0x28,0x24,0x22,0x20,0x1F,0x1E,0x1D,
    0x0E,0x0C,0x0A,0x08,0x06,0x05,0x04,0x02,0x00,0xFF,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF
};

static uint8_t s_ready;
static uint8_t s_touch_type;
static uint8_t s_status;
static uint16_t s_x[TOUCH_MAX_POINTS];
static uint16_t s_y[TOUCH_MAX_POINTS];

static uint8_t gt9147_read_reg(uint16_t reg, uint8_t *buffer, uint8_t length)
{
    return iic_read_reg16(&s_touch_iic, GT9147_I2C_ADDRESS, reg, buffer, length);
}

static uint8_t gt9147_write_reg(uint16_t reg, const uint8_t *buffer,
                                uint8_t length)
{
    return iic_write_reg16(&s_touch_iic, GT9147_I2C_ADDRESS, reg, buffer, length);
}

static uint8_t gt9147_send_config(void)
{
    uint8_t checksum[2];
    uint8_t sum = 0U;
    uint8_t index;
    uint8_t status;

    for (index = 0U; index < sizeof(s_gt9147_config); index++)
        sum = (uint8_t)(sum + s_gt9147_config[index]);
    checksum[0] = (uint8_t)(~sum + 1U);
    checksum[1] = 0x01U;
    status = gt9147_write_reg(GT9147_CONFIG_REG, s_gt9147_config,
                              sizeof(s_gt9147_config));
    if (status != 0U)
        return status;

    return gt9147_write_reg(GT9147_CHECK_REG, checksum, sizeof(checksum));
}

static uint8_t touch_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOF, ENABLE);

    gpio.GPIO_Pin = TOUCH_RST_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TOUCH_RST_PORT, &gpio);
    GPIO_SetBits(TOUCH_RST_PORT, TOUCH_RST_PIN);

    gpio.GPIO_Pin = TOUCH_INT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(TOUCH_INT_PORT, &gpio);

    return iic_init(&s_touch_iic);
}

static uint8_t gt9147_init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t id[5];
    uint8_t value;
    uint8_t status;

    status = touch_gpio_init();
    if (status != 0U)
        return status;
    GPIO_ResetBits(TOUCH_RST_PORT, TOUCH_RST_PIN);
    delay_ms(10U);
    GPIO_SetBits(TOUCH_RST_PORT, TOUCH_RST_PIN);
    delay_ms(10U);

    gpio.GPIO_Pin = TOUCH_INT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(TOUCH_INT_PORT, &gpio);
    GPIO_ResetBits(TOUCH_INT_PORT, TOUCH_INT_PIN);
    delay_ms(100U);

    status = gt9147_read_reg(GT9147_PID_REG, id, 4U);
    if (status != 0U)
        return status;

    id[4] = 0U;
    if (strcmp((const char *)id, "9147") != 0)
        return 2U;

    value = 0x02U;
    status = gt9147_write_reg(GT9147_CTRL_REG, &value, 1U);
    if (status != 0U)
        return status;

    status = gt9147_read_reg(GT9147_CONFIG_REG, &value, 1U);
    if (status != 0U)
        return status;

    if (value < 0x60U)
    {
        status = gt9147_send_config();
        if (status != 0U)
            return status;
    }

    delay_ms(10U);
    value = 0x00U;
    return gt9147_write_reg(GT9147_CTRL_REG, &value, 1U);
}

uint8_t touch_init(void)
{
    uint8_t index;
    uint8_t status;

    status = gt9147_init();
    if (status != 0U)
    {
        s_ready = 0U;
        return status;
    }

    s_ready = 1U;
    s_touch_type = 0x80U;
    s_status = 0U;
    for (index = 0U; index < TOUCH_MAX_POINTS; index++)
    {
        s_x[index] = 0xFFFFU;
        s_y[index] = 0xFFFFU;
    }
    return 0U;
}

uint8_t touch_scan(touch_point_t *point)
{
    uint8_t mode = 0U;
    uint8_t buffer[4];
    uint8_t count;
    uint8_t index;
    uint8_t temp;
    uint8_t old_status;
    uint8_t status;
    static uint8_t tick;

    if (point == 0 || s_ready == 0U)
        return 1U;

    point->pressed = 0U;
    point->x = 0xFFFFU;
    point->y = 0xFFFFU;
    tick++;
    if ((tick % 10U) == 0U || tick < 10U)
    {
        status = gt9147_read_reg(GT9147_STATUS_REG, &mode, 1U);
        if (status != 0U)
            return status;

        if ((mode & 0x80U) != 0U && (mode & 0x0FU) < 6U)
        {
            temp = 0U;
            status = gt9147_write_reg(GT9147_STATUS_REG, &temp, 1U);
            if (status != 0U)
                return status;
        }
        count = mode & 0x0FU;
        if (count != 0U && count < 6U)
        {
            temp = (uint8_t)(0xFFU << count);
            old_status = s_status;
            s_status = (uint8_t)((~temp) | TOUCH_PRES_DOWN |
                                 TOUCH_CATH_PRES);
            s_x[4] = s_x[0];
            s_y[4] = s_y[0];
            for (index = 0U; index < TOUCH_MAX_POINTS; index++)
            {
                if ((s_status & (1U << index)) != 0U)
                {
                    status = gt9147_read_reg(s_point_registers[index], buffer, 4U);
                    if (status != 0U)
                        return status;
                    if ((s_touch_type & 0x01U) != 0U)
                    {
                        s_y[index] = (uint16_t)buffer[1] << 8 | buffer[0];
                        s_x[index] = 800U -
                                     ((uint16_t)buffer[3] << 8 | buffer[2]);
                    }
                    else
                    {
                        s_x[index] = (uint16_t)buffer[1] << 8 | buffer[0];
                        s_y[index] = (uint16_t)buffer[3] << 8 | buffer[2];
                    }
                }
            }
            if (s_x[0] >= TOUCH_WIDTH || s_y[0] >= TOUCH_HEIGHT)
            {
                if (count > 1U)
                {
                    s_x[0] = s_x[1];
                    s_y[0] = s_y[1];
                    tick = 0U;
                }
                else
                {
                    s_x[0] = s_x[4];
                    s_y[0] = s_y[4];
                    mode = 0x80U;
                    s_status = old_status;
                }
            }
            else
            {
                tick = 0U;
            }
        }
    }

    if ((mode & 0x8FU) == 0x80U)
    {
        if ((s_status & TOUCH_PRES_DOWN) != 0U)
            s_status &= (uint8_t)~TOUCH_PRES_DOWN;
        else
        {
            s_x[0] = 0xFFFFU;
            s_y[0] = 0xFFFFU;
            s_status &= 0xE0U;
        }
    }

    if ((s_status & TOUCH_PRES_DOWN) != 0U &&
        s_x[0] < TOUCH_WIDTH && s_y[0] < TOUCH_HEIGHT)
    {
        point->x = s_x[0];
        point->y = s_y[0];
        point->pressed = 1U;
    }
    if (tick > 240U)
        tick = 10U;
    return 0U;
}

uint8_t touch_is_ready(void)
{
    return s_ready;
}
