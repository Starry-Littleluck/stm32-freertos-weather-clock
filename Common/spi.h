#ifndef __SPI_H
#define __SPI_H

#include "stm32f10x.h"

#define SPI_SOFT_MODE0 0U
#define SPI_SOFT_MODE3 3U

struct spi_desc
{
    GPIO_TypeDef *Sck_port;
    uint16_t Sck_pin;
    GPIO_TypeDef *Mosi_port;
    uint16_t Mosi_pin;
    GPIO_TypeDef *Miso_port;
    uint16_t Miso_pin;
    uint8_t Mode;
};

typedef struct spi_desc *spi_desc_t;

uint8_t spi_init(spi_desc_t spi);
uint8_t spi_transfer_byte(spi_desc_t spi, uint8_t data);

#endif /* __SPI_H */
