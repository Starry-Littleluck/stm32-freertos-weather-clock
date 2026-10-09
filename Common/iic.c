#include "iic.h"

#include "delay.h"
#include "stm32f10x_gpio.h"

static void iic_write_scl(const iic_desc_t iic, BitAction level)
{
    GPIO_WriteBit(iic->Scl_port, iic->Scl_pin, level);
    delay_us(5U);
}

static void iic_write_sda(const iic_desc_t iic, BitAction level)
{
    GPIO_WriteBit(iic->Sda_port, iic->Sda_pin, level);
}

static uint8_t iic_read_sda(const iic_desc_t iic)
{
    return GPIO_ReadInputDataBit(iic->Sda_port, iic->Sda_pin) != Bit_RESET;
}

static void iic_start(const iic_desc_t iic)
{
    iic_write_sda(iic, Bit_SET);
    iic_write_scl(iic, Bit_SET);
    iic_write_sda(iic, Bit_RESET);
    delay_us(5U);
    iic_write_scl(iic, Bit_RESET);
}

static void iic_stop(const iic_desc_t iic)
{
    iic_write_sda(iic, Bit_RESET);
    iic_write_scl(iic, Bit_SET);
    iic_write_sda(iic, Bit_SET);
    delay_us(5U);
}

static uint8_t iic_write_byte(const iic_desc_t iic, uint8_t data)
{
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++)
    {
        iic_write_sda(iic, (data & (uint8_t)(0x80U >> bit)) != 0U
                           ? Bit_SET : Bit_RESET);
        iic_write_scl(iic, Bit_SET);
        iic_write_scl(iic, Bit_RESET);
    }

    iic_write_sda(iic, Bit_SET);
    iic_write_scl(iic, Bit_SET);
    bit = iic_read_sda(iic);
    iic_write_scl(iic, Bit_RESET);
    return bit == 0U ? 0U : 2U;
}

static uint8_t iic_read_byte(const iic_desc_t iic, uint8_t *data,
                             uint8_t send_ack)
{
    uint8_t bit;
    uint8_t value = 0U;

    iic_write_sda(iic, Bit_SET);
    for (bit = 0U; bit < 8U; bit++)
    {
        value <<= 1;
        iic_write_scl(iic, Bit_SET);
        if (iic_read_sda(iic) != 0U)
            value |= 1U;
        iic_write_scl(iic, Bit_RESET);
    }

    iic_write_sda(iic, send_ack != 0U ? Bit_RESET : Bit_SET);
    iic_write_scl(iic, Bit_SET);
    iic_write_scl(iic, Bit_RESET);
    iic_write_sda(iic, Bit_SET);
    *data = value;
    return 0U;
}

static uint8_t iic_write_data(const iic_desc_t iic, const uint8_t *data,
                              uint16_t length)
{
    uint16_t index;
    uint8_t status;

    for (index = 0U; index < length; index++)
    {
        status = iic_write_byte(iic, data[index]);
        if (status != 0U)
            return status;
    }
    return 0U;
}

uint8_t iic_init(iic_desc_t iic)
{
    GPIO_InitTypeDef gpio;

    if (iic == 0 || iic->Scl_port == 0 || iic->Sda_port == 0 ||
        iic->Scl_pin == 0U || iic->Sda_pin == 0U)
        return 1U;

    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    gpio.GPIO_Pin = iic->Scl_pin;
    GPIO_Init(iic->Scl_port, &gpio);
    if (iic->Sda_port == iic->Scl_port)
    {
        gpio.GPIO_Pin = iic->Sda_pin;
        GPIO_Init(iic->Sda_port, &gpio);
    }
    else
    {
        gpio.GPIO_Pin = iic->Sda_pin;
        GPIO_Init(iic->Sda_port, &gpio);
    }

    iic_write_sda(iic, Bit_SET);
    iic_write_scl(iic, Bit_SET);
    delay_us(5U);

    /* 软件 I2C 释放总线后，两根线都必须回到高电平。否则通常是没有上拉、
       接线错误或外设把总线拉死，继续通信只会产生假 ACK 和全 0 数据。 */
    if (GPIO_ReadInputDataBit(iic->Scl_port, iic->Scl_pin) == Bit_RESET ||
        GPIO_ReadInputDataBit(iic->Sda_port, iic->Sda_pin) == Bit_RESET)
        return 3U;
    return 0U;
}

uint8_t iic_write(iic_desc_t iic, uint8_t address,
                  const uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;

    iic_start(iic);
    status = iic_write_byte(iic, (uint8_t)(address << 1));
    if (status == 0U)
        status = iic_write_data(iic, data, length);
    iic_stop(iic);
    return status;
}

uint8_t iic_write_reg(iic_desc_t iic, uint8_t address, uint8_t reg,
                      const uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;

    iic_start(iic);
    status = iic_write_byte(iic, (uint8_t)(address << 1));
    if (status == 0U)
        status = iic_write_byte(iic, reg);
    if (status == 0U)
        status = iic_write_data(iic, data, length);
    iic_stop(iic);
    return status;
}

uint8_t iic_read_reg(iic_desc_t iic, uint8_t address, uint8_t reg,
                     uint8_t *data, uint16_t length)
{
    uint16_t index;
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;
    if (length == 0U)
        return 0U;

    iic_start(iic);
    status = iic_write_byte(iic, (uint8_t)(address << 1));
    if (status == 0U)
        status = iic_write_byte(iic, reg);
    if (status == 0U)
    {
        iic_start(iic);
        status = iic_write_byte(iic, (uint8_t)((address << 1) | 1U));
    }
    if (status == 0U)
    {
        for (index = 0U; index < length; index++)
        {
            status = iic_read_byte(iic, &data[index],
                                   (index + 1U < length) ? 1U : 0U);
            if (status != 0U)
                break;
        }
    }
    iic_stop(iic);
    return status;
}

uint8_t iic_write_reg16(iic_desc_t iic, uint8_t address, uint16_t reg,
                        const uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;

    iic_start(iic);
    status = iic_write_byte(iic, (uint8_t)(address << 1));
    if (status == 0U)
        status = iic_write_byte(iic, (uint8_t)(reg >> 8));
    if (status == 0U)
        status = iic_write_byte(iic, (uint8_t)reg);
    if (status == 0U)
        status = iic_write_data(iic, data, length);
    iic_stop(iic);
    return status;
}

uint8_t iic_read_reg16(iic_desc_t iic, uint8_t address, uint16_t reg,
                       uint8_t *data, uint16_t length)
{
    uint16_t index;
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;
    if (length == 0U)
        return 0U;

    iic_start(iic);
    status = iic_write_byte(iic, (uint8_t)(address << 1));
    if (status == 0U)
        status = iic_write_byte(iic, (uint8_t)(reg >> 8));
    if (status == 0U)
        status = iic_write_byte(iic, (uint8_t)reg);
    if (status == 0U)
    {
        iic_start(iic);
        status = iic_write_byte(iic, (uint8_t)((address << 1) | 1U));
    }
    if (status == 0U)
    {
        for (index = 0U; index < length; index++)
        {
            status = iic_read_byte(iic, &data[index],
                                   (index + 1U < length) ? 1U : 0U);
            if (status != 0U)
                break;
        }
    }
    iic_stop(iic);
    return status;
}
