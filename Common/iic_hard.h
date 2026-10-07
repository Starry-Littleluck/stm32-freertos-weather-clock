#ifndef __IIC_HARD_H
#define __IIC_HARD_H

#include "stm32f10x.h"

struct iic_hard_desc
{
    I2C_TypeDef *Instance;   /* I2C外设地址 */
    GPIO_TypeDef *Gpio_port; /* GPIO端口地址 */
    uint16_t Pin_scl;        /* SCL引脚 */
    uint16_t Pin_sda;        /* SDA引脚 */
    uint32_t Speed;          /* I2C时钟频率(Hz) */
    uint32_t Timeout;        /* I2C操作超时时间(ms) */
};

typedef struct iic_hard_desc *iic_hard_desc_t;

uint8_t iic_hard_init(iic_hard_desc_t iic);                                                                          /* 初始化硬件I2C */
uint8_t iic_hard_write(iic_hard_desc_t iic, uint8_t address, const uint8_t *data, uint16_t length);                  /* 硬件I2C写数据 */
uint8_t iic_hard_write_reg(iic_hard_desc_t iic, uint8_t address, uint8_t reg, const uint8_t *data, uint16_t length); /* 硬件I2C写寄存器 */
uint8_t iic_hard_read_reg(iic_hard_desc_t iic, uint8_t address, uint8_t reg, uint8_t *data, uint16_t length);        /* 硬件I2C读寄存器 */
void iic_hard_get_debug(uint8_t *stage, uint16_t *sr1, uint16_t *sr2,
                        uint16_t *cr1, uint16_t *cr2);                                                               /* 获取最近一次失败位置 */

#endif /* __IIC_HARD_H */
