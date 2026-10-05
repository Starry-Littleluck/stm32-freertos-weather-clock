#ifndef __UART_H
#define __UART_H

#include "stm32f10x.h"
#include <stdint.h>
#include <stdio.h>

/* UART 描述结构体 */
struct uart_desc
{
    /* ---------- 外设 ---------- */
    USART_TypeDef *instance; /* USART1 / USART2 / USART3 / UART4 / UART5 */
    GPIO_TypeDef *gpio_port; /* GPIOA / GPIOB / ...                     */
    uint16_t pin_tx;         /* 例如 GPIO_Pin_9                         */
    uint16_t pin_rx;         /* 例如 GPIO_Pin_10                        */

    /* ---------- DMA 通道（IDLE 中断里计算写指针用） ---------- */
    DMA_Channel_TypeDef *dma_channel; /* 例如 DMA1_Channel5 */

    /* ---------- DMA 硬件循环缓冲区 ---------- */
    uint8_t *rx_dma_buffer;       /* 用户提供，DMA 循环写入 */
    uint16_t rx_dma_size;         /* 大小，建议 2 的幂 */
    volatile uint16_t rx_dma_pos; /* 软件记录：上次已搬走的 DMA 位置 */

    /* ---------- 软件 FIFO（内联） ---------- */
    uint8_t *rx_fifo_buffer;   /* 用户提供，容量必须为 2 的幂 */
    uint16_t rx_fifo_size;     /* 容量 */
    uint16_t rx_fifo_mask;     /* = size - 1，由 UART_Init 计算 */
    volatile uint16_t rx_head; /* 写指针（中断里更新） */
    volatile uint16_t rx_tail; /* 读指针（主循环更新） */
};

typedef struct uart_desc *uart_desc_t; /* 句柄类型 */

void uart_init(uart_desc_t uart);                                       /* 只初始化软件状态（FIFO 索引、mask、rx_dma_pos） */
void uart_send(uart_desc_t uart, const uint8_t *data, uint16_t length); /* 主循环里调用，发送数据 */
void uart_send_string(uart_desc_t uart, const char *string);            /* 主循环里调用，发送字符串 */
void uart_handle(uart_desc_t uart);                                     /* IDLE 中断里调用，搬运 DMA 数据到 FIFO */

void uart_dma_push(uart_desc_t uart, uint16_t position);                /* DMA 中断里调用，更新写指针 */
uint16_t uart_available(const uart_desc_t uart);                        /* 主循环里调用，返回可读字节数 */
uint16_t uart_read(uart_desc_t uart, uint8_t *buf, uint16_t len);       /* 主循环里调用，从 FIFO 里读数据 */
uint16_t uart_readline(uart_desc_t uart, uint8_t *line, uint16_t size); /* 主循环里调用，从 FIFO 里读一行数据，返回字节数 */

#endif /* __UART_H */
