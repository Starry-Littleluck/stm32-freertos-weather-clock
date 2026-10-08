#include "spi_hard.h"

#include "stm32f10x_spi.h"

uint8_t spi_hard_set_mode(spi_hard_desc_t spi, uint8_t mode)
{
    SPI_InitTypeDef config;

    if (spi == 0 || spi->Instance == 0 ||
        (mode != SPI_HARD_MODE0 && mode != SPI_HARD_MODE3))
    {
        return 1U;
    }

    SPI_Cmd(spi->Instance, DISABLE);
    SPI_StructInit(&config);
    config.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    config.SPI_Mode = SPI_Mode_Master;
    config.SPI_DataSize = SPI_DataSize_8b;
    config.SPI_CPOL = mode == SPI_HARD_MODE3 ? SPI_CPOL_High : SPI_CPOL_Low;
    config.SPI_CPHA = mode == SPI_HARD_MODE3 ? SPI_CPHA_2Edge : SPI_CPHA_1Edge;
    config.SPI_NSS = SPI_NSS_Soft;
    config.SPI_BaudRatePrescaler = spi->Baud_prescaler;
    config.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(spi->Instance, &config);
    SPI_Cmd(spi->Instance, ENABLE);
    spi->Mode = mode;
    return 0U;
}

uint8_t spi_hard_init(spi_hard_desc_t spi)
{
    if (spi == 0 || spi->Instance == 0)
    {
        return 1U;
    }
    return spi_hard_set_mode(spi, spi->Mode);
}

uint8_t spi_hard_transfer_byte(spi_hard_desc_t spi, uint8_t data)
{
    if (spi == 0 || spi->Instance == 0)
    {
        return 0U;
    }
    while (SPI_I2S_GetFlagStatus(spi->Instance, SPI_I2S_FLAG_TXE) == RESET)
    {
    }
    SPI_I2S_SendData(spi->Instance, data);
    while (SPI_I2S_GetFlagStatus(spi->Instance, SPI_I2S_FLAG_RXNE) == RESET)
    {
    }
    return (uint8_t)SPI_I2S_ReceiveData(spi->Instance);
}
