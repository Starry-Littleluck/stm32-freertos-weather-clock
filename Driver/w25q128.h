#ifndef __W25Q128_H
#define __W25Q128_H

#include "stm32f10x.h"

#define W25Q128_PAGE_SIZE 256U
#define W25Q128_SECTOR_SIZE 4096U
#define W25Q128_CAPACITY (16UL * 1024UL * 1024UL)
#define W25Q128_JEDEC_ID 0xEF4018UL

uint8_t w25q128_init(void);                                                    /* 初始化 W25Q128 芯片，返回 0 表示成功，非 0 表示失败 */
uint32_t w25q128_read_id(void);                                                /* 读取芯片 ID，返回值为 0xEF4018 表示 W25Q128 芯片 */
uint8_t w25q128_read_status(void);                                             /* 读取状态寄存器 */
uint8_t w25q128_read(uint32_t address, uint8_t *data, uint16_t length);        /* 从指定地址读取数据，返回 0 表示成功，非 0 表示失败 */
uint8_t w25q128_write(uint32_t address, const uint8_t *data, uint16_t length); /* 向指定地址写入数据，返回 0 表示成功，非 0 表示失败 */
uint8_t w25q128_erase_sector(uint32_t address);                                /* 擦除指定扇区，返回 0 表示成功，非 0 表示失败 */

#endif /* __W25Q128_H */
