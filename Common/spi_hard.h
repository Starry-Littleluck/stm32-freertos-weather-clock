#ifndef __SPI_HARD_H
#define __SPI_HARD_H

#include "stm32f10x.h"

#define SPI_HARD_MODE0 0U
#define SPI_HARD_MODE3 3U

struct spi_hard_desc
{
    SPI_TypeDef *Instance;
    uint16_t Baud_prescaler;
    uint8_t Mode;
};

typedef struct spi_hard_desc *spi_hard_desc_t;

uint8_t spi_hard_init(spi_hard_desc_t spi);
uint8_t spi_hard_set_mode(spi_hard_desc_t spi, uint8_t mode);
uint8_t spi_hard_transfer_byte(spi_hard_desc_t spi, uint8_t data);

#endif /* __SPI_HARD_H */
