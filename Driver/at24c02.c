#include "at24c02.h"

#include "delay.h"
#include "iic.h"

static struct iic_desc s_at24c02_iic =
{
    .Scl_port = GPIOB,
    .Scl_pin = GPIO_Pin_6,
    .Sda_port = GPIOB,
    .Sda_pin = GPIO_Pin_7
};

static uint8_t at24c02_valid_range(uint16_t address, uint16_t length)
{
    if (address >= AT24C02_SIZE)
        return 0U;
    if (length > (uint16_t)(AT24C02_SIZE - address))
        return 0U;
    return 1U;
}

uint8_t at24c02_init(void)
{
    return iic_init(&s_at24c02_iic);
}

uint8_t at24c02_write_byte(uint16_t address, uint8_t data)
{
    uint8_t status;

    if (at24c02_valid_range(address, 1U) == 0U)
        return 1U;

    status = iic_write_reg(&s_at24c02_iic, AT24C02_I2C_ADDRESS,
                           (uint8_t)address, &data, 1U);
    if (status == 0U)
        delay_ms(10U);
    return status;
}

uint8_t at24c02_read_byte(uint16_t address, uint8_t *data)
{
    if (data == 0 || at24c02_valid_range(address, 1U) == 0U)
        return 1U;

    return iic_read_reg(&s_at24c02_iic, AT24C02_I2C_ADDRESS,
                        (uint8_t)address, data, 1U);
}

uint8_t at24c02_write(uint16_t address, const uint8_t *data,uint16_t length)
{
    uint16_t page_left;
    uint16_t count;
    uint8_t status;

    if (length == 0U)
        return 0U;
    if (data == 0 || at24c02_valid_range(address, length) == 0U)
        return 1U;

    while (length > 0U)
    {
        page_left = (uint16_t)(AT24C02_PAGE_SIZE -
                               (address % AT24C02_PAGE_SIZE));
        count = length < page_left ? length : page_left;
        status = iic_write_reg(&s_at24c02_iic, AT24C02_I2C_ADDRESS,
                               (uint8_t)address, data, count);
        if (status != 0U)
            return status;

        delay_ms(10U);
        address = (uint16_t)(address + count);
        data += count;
        length = (uint16_t)(length - count);
    }
    return 0U;
}

uint8_t at24c02_read(uint16_t address, uint8_t *data, uint16_t length)
{
    uint16_t count;
    uint8_t status;

    if (length == 0U)
        return 0U;
    if (data == 0 || at24c02_valid_range(address, length) == 0U)
        return 1U;

    while (length > 0U)
    {
        count = (uint16_t)(AT24C02_SIZE - address);
        count = length < count ? length : count;
        status = iic_read_reg(&s_at24c02_iic, AT24C02_I2C_ADDRESS,
                              (uint8_t)address, data, count);
        if (status != 0U)
            return status;

        address = (uint16_t)(address + count);
        data += count;
        length = (uint16_t)(length - count);
    }
    return 0U;
}

uint8_t at24c02_check(void)
{
    uint8_t data;
    uint8_t status;

    status = at24c02_read_byte(AT24C02_SIZE - 1U, &data);
    if (status != 0U)
        return status;
    if (data != 0x55U)
    {
        status = at24c02_write_byte(AT24C02_SIZE - 1U, 0x55U);
        if (status != 0U)
            return status;
        status = at24c02_read_byte(AT24C02_SIZE - 1U, &data);
    }
    return (status == 0U && data == 0x55U) ? 0U : 1U;
}
