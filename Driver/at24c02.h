#ifndef __AT24C02_H
#define __AT24C02_H

#include "stm32f10x.h"

#define AT24C02_I2C_ADDRESS 0x50U
#define AT24C02_SIZE 256U
#define AT24C02_PAGE_SIZE 8U

uint8_t at24c02_init(void);
uint8_t at24c02_write_byte(uint16_t address, uint8_t data);
uint8_t at24c02_read_byte(uint16_t address, uint8_t *data);
uint8_t at24c02_write(uint16_t address, const uint8_t *data,
                      uint16_t length);
uint8_t at24c02_read(uint16_t address, uint8_t *data, uint16_t length);
uint8_t at24c02_check(void);

#endif /* __AT24C02_H */
