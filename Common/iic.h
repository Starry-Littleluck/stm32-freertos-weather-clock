#ifndef __IIC_H
#define __IIC_H

#include "stm32f10x.h"

struct iic_desc
{
    GPIO_TypeDef *Scl_port;
    uint16_t Scl_pin;
    GPIO_TypeDef *Sda_port;
    uint16_t Sda_pin;
};

typedef struct iic_desc *iic_desc_t;

uint8_t iic_init(iic_desc_t iic);                                                                          /* 初始化IIC */
uint8_t iic_write(iic_desc_t iic, uint8_t address, const uint8_t *data, uint16_t length);                  /* IIC写数据 */
uint8_t iic_write_reg(iic_desc_t iic, uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length); /* IIC写寄存器 */
uint8_t iic_read_reg(iic_desc_t iic, uint8_t address, uint8_t reg, uint8_t *data, uint16_t length);        /* IIC读寄存器 */
uint8_t iic_write_reg16(iic_desc_t iic, uint8_t address, uint16_t reg, const uint8_t *data, uint16_t length); /* IIC写16位寄存器 */
uint8_t iic_read_reg16(iic_desc_t iic, uint8_t address, uint16_t reg, uint8_t *data, uint16_t length);        /* IIC读16位寄存器 */

#endif /* __IIC_H */
