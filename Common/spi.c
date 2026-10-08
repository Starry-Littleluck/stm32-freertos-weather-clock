#include "spi.h"

#include "delay.h"
#include "stm32f10x_gpio.h"

static void spi_write_sck(const spi_desc_t spi, BitAction level)
{
    GPIO_WriteBit(spi->Sck_port, spi->Sck_pin, level);
    delay_us(1U);
}

static void spi_write_mosi(const spi_desc_t spi, BitAction level)
{
    GPIO_WriteBit(spi->Mosi_port, spi->Mosi_pin, level);
}

static uint8_t spi_read_miso(const spi_desc_t spi)
{
    return GPIO_ReadInputDataBit(spi->Miso_port, spi->Miso_pin) != Bit_RESET;
}

uint8_t spi_init(spi_desc_t spi)
{
    GPIO_InitTypeDef gpio;

    if (spi == 0 || spi->Sck_port == 0 || spi->Mosi_port == 0 ||
        spi->Miso_port == 0 || spi->Sck_pin == 0U ||
        spi->Mosi_pin == 0U || spi->Miso_pin == 0U ||
        (spi->Mode != SPI_SOFT_MODE0 && spi->Mode != SPI_SOFT_MODE3))
    {
        return 1U;
    }

    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = spi->Sck_pin;
    GPIO_Init(spi->Sck_port, &gpio);
    gpio.GPIO_Pin = spi->Mosi_pin;
    GPIO_Init(spi->Mosi_port, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin = spi->Miso_pin;
    GPIO_Init(spi->Miso_port, &gpio);

    spi_write_sck(spi, spi->Mode == SPI_SOFT_MODE3 ? Bit_SET : Bit_RESET);
    spi_write_mosi(spi, Bit_RESET);
    return 0U;
}

uint8_t spi_transfer_byte(spi_desc_t spi, uint8_t data)
{
    uint8_t bit;
    uint8_t received = 0U;

    if (spi == 0)
    {
        return 0U;
    }

    for (bit = 0U; bit < 8U; bit++)
    {
        spi_write_mosi(spi, (data & 0x80U) != 0U ? Bit_SET : Bit_RESET);
        data <<= 1;

        if (spi->Mode == SPI_SOFT_MODE3)
        {
            spi_write_sck(spi, Bit_RESET);
            received <<= 1;
            if (spi_read_miso(spi) != 0U)
            {
                received |= 1U;
            }
            spi_write_sck(spi, Bit_SET);
        }
        else
        {
            spi_write_sck(spi, Bit_SET);
            received <<= 1;
            if (spi_read_miso(spi) != 0U)
            {
                received |= 1U;
            }
            spi_write_sck(spi, Bit_RESET);
        }
    }
    return received;
}
