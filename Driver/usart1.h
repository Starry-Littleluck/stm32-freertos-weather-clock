#ifndef __USART1_H
#define __USART1_H

#include "uart.h"
#include <stdint.h>

void usart1_init(uint32_t baudrate);                    /* 初始化 USART1：填充对象 + 配置 GPIO / USART / DMA / NVIC / IDLE。 */
void usart1_send(const uint8_t *data, uint16_t length); /* 阻塞发送指定长度的数据。 */
void usart1_send_string(const char *string);            /* 阻塞发送字符串。 */

uint16_t usart1_available(void);                        /* 返回 FIFO 中当前可读字节数。 */
uint16_t usart1_read(uint8_t *buf, uint16_t len);       /* 从 FIFO 读一段数据。 */
uint16_t usart1_readline(uint8_t *line, uint16_t size); /* 从 FIFO 读取一行（以 '\n' 结束，自动去掉行尾 '\r'）。 */

#endif /* __USART1_H */
