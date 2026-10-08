#include "w25q128.h"
#include "delay.h"
#include "spi_hard.h"

#define W25_CMD_WRITE_ENABLE 0x06U
#define W25_CMD_READ_STATUS 0x05U
#define W25_CMD_READ_ID 0x9FU
#define W25_CMD_READ_DATA 0x03U
#define W25_CMD_PAGE_PROGRAM 0x02U
#define W25_CMD_SECTOR_ERASE 0x20U
#define W25_STATUS_BUSY 0x01U
#define W25_WAIT_TIMEOUT_MS 5000U

#define W25_CS_PORT GPIOB
#define W25_CS_PIN GPIO_Pin_12

static struct spi_hard_desc s_w25_spi =
{
    SPI2,
    SPI_BaudRatePrescaler_8,
    SPI_HARD_MODE3
};

static void w25_cs(uint8_t level)
{
    GPIO_WriteBit(W25_CS_PORT, W25_CS_PIN,
                  level != 0U ? Bit_SET : Bit_RESET);
}

static uint8_t w25_transfer(uint8_t data)
{
    return spi_hard_transfer_byte(&s_w25_spi, data);
}

static void w25_send_address(uint32_t address)
{
    w25_transfer((uint8_t)(address >> 16));
    w25_transfer((uint8_t)(address >> 8));
    w25_transfer((uint8_t)address);
}

static void w25_write_enable(void)
{
    w25_cs(0U);
    w25_transfer(W25_CMD_WRITE_ENABLE);
    w25_cs(1U);
}

static uint8_t w25_wait_busy(void)
{
    uint32_t elapsed = 0U;

    while ((w25q128_read_status() & W25_STATUS_BUSY) != 0U)
    {
        delay_ms(1U);
        elapsed++;
        if (elapsed >= W25_WAIT_TIMEOUT_MS)
        {
            return 1U;
        }
    }
    return 0U;
}

uint8_t w25q128_init(void)
{
    GPIO_InitTypeDef gpio;
    uint8_t status;

    gpio.GPIO_Pin = W25_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(W25_CS_PORT, &gpio);
    w25_cs(1U);

    gpio.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_14;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &gpio);

    status = spi_hard_init(&s_w25_spi);
    w25_cs(1U);
    return status;
}

uint32_t w25q128_read_id(void)
{
    uint32_t id;

    w25_cs(0U);
    w25_transfer(W25_CMD_READ_ID);
    id = (uint32_t)w25_transfer(0xFFU) << 16;
    id |= (uint32_t)w25_transfer(0xFFU) << 8;
    id |= w25_transfer(0xFFU);
    w25_cs(1U);
    return id;
}

uint8_t w25q128_read_status(void)
{
    uint8_t status;

    w25_cs(0U);
    w25_transfer(W25_CMD_READ_STATUS);
    status = w25_transfer(0xFFU);
    w25_cs(1U);
    return status;
}

uint8_t w25q128_read(uint32_t address, uint8_t *data, uint16_t length)
{
    if ((data == 0 && length != 0U) || address >= W25Q128_CAPACITY ||
        length > W25Q128_CAPACITY - address)
    {
        return 1U;
    }
    if (length == 0U)
    {
        return 0U;
    }

    w25_cs(0U);
    w25_transfer(W25_CMD_READ_DATA);
    w25_send_address(address);
    while (length-- != 0U)
    {
        *data++ = w25_transfer(0xFFU);
    }
    w25_cs(1U);
    return 0U;
}

static uint8_t w25_write_page(uint32_t address, const uint8_t *data,
                              uint16_t length)
{
    w25_write_enable();
    w25_cs(0U);
    w25_transfer(W25_CMD_PAGE_PROGRAM);
    w25_send_address(address);
    while (length-- != 0U)
    {
        w25_transfer(*data++);
    }
    w25_cs(1U);
    return w25_wait_busy();
}

uint8_t w25q128_write(uint32_t address, const uint8_t *data, uint16_t length)
{
    uint16_t page_left;
    uint16_t count;

    if ((data == 0 && length != 0U) || address >= W25Q128_CAPACITY ||
        length > W25Q128_CAPACITY - address)
    {
        return 1U;
    }

    while (length != 0U)
    {
        page_left = (uint16_t)(W25Q128_PAGE_SIZE -
                               (address % W25Q128_PAGE_SIZE));
        count = length < page_left ? length : page_left;
        if (w25_write_page(address, data, count) != 0U)
        {
            return 1U;
        }
        address += count;
        data += count;
        length = (uint16_t)(length - count);
    }
    return 0U;
}

uint8_t w25q128_erase_sector(uint32_t address)
{
    if (address >= W25Q128_CAPACITY)
    {
        return 1U;
    }
    w25_write_enable();
    w25_cs(0U);
    w25_transfer(W25_CMD_SECTOR_ERASE);
    w25_send_address(address);
    w25_cs(1U);
    return w25_wait_busy();
}
