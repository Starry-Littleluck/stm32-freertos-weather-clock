#include "uart.h"
#include <string.h>

/* ============================================================
 *                     FIFO 内部辅助函数
 * ============================================================ */
/**
 * @brief 计算 FIFO 索引前进 n 个位置后的新索引。
 */
static uint16_t fifo_new_index(const uart_desc_t uart, uint16_t index, uint16_t n)
{
    return (uint16_t)((index + n) & uart->rx_fifo_mask);
}

/**
 * @brief 把一段数据复制到 FIFO。
 * @param uart  UART 对象
 * @param index 写入起点索引
 * @param data   源数据
 * @param len   数据长度
 */
static void fifo_data_in(uart_desc_t uart, uint16_t index, const uint8_t *data, uint16_t len)
{
    /* 先写入 index 到 FIFO 尾部的空间 */
    uint16_t first = (len < (uint16_t)(uart->rx_fifo_size - index)) ? len : (uint16_t)(uart->rx_fifo_size - index);

    /* 复制数据到 FIFO */
    memcpy(&uart->rx_fifo_buffer[index], data, first);
    if (len > first) /* 如果数据长度超过了 FIFO 尾部空间，则从 FIFO 头部继续写入 */
        memcpy(uart->rx_fifo_buffer, &data[first], (uint16_t)(len - first));
}

/**
 * @brief 从 FIFO 读出一段数据。
 * @param uart  UART 对象
 * @param index 读出起点索引
 * @param data   目标缓冲区
 * @param len   数据长度
 */
static void fifo_data_out(const uart_desc_t uart, uint16_t index, uint8_t *data, uint16_t len)
{
    /* 先读出 index 到 FIFO 尾部的空间 */
    uint16_t first = (len < (uint16_t)(uart->rx_fifo_size - index)) ? len : (uint16_t)(uart->rx_fifo_size - index);

    /* 复制数据到目标缓冲区 */
    memcpy(data, &uart->rx_fifo_buffer[index], first);
    if (len > first) /* 如果数据长度超过了 FIFO 尾部空间，则从 FIFO 头部继续读出 */
        memcpy(&data[first], uart->rx_fifo_buffer, (uint16_t)(len - first));
}

/**
 * @brief 把一段数据写入 FIFO。
 * @param uart UART 对象
 * @param data 源数据
 * @param len  数据长度
 * @return 实际写入的字节数（可能小于 len，因为 FIFO 空间有限）
 */
static uint16_t fifo_write(uart_desc_t uart, const uint8_t *data, uint16_t len)
{
    /* 计算 FIFO 剩余空间 */
    uint16_t free_size = (uint16_t)(uart->rx_fifo_size - uart_available(uart) - 1);
    /* 实际写入的字节数 = min(len, free_size) */
    uint16_t write_size = (len < free_size) ? len : free_size;

    fifo_data_in(uart, uart->rx_head, data, write_size);             /* 写入数据到 FIFO */
    uart->rx_head = fifo_new_index(uart, uart->rx_head, write_size); /* 更新写指针 */
    return write_size;                                               /* 返回实际写入的字节数 */
}

/* ============================================================
 *                       对象初始化
 * ============================================================ */

/**
 * @brief 初始化 UART 对象的软件状态。
 *        只做 FIFO 索引清零和 mask 计算；GPIO / DMA / USART / NVIC 由
 *        调用者在自己的文件里配置。
 */
void uart_init(uart_desc_t uart)
{
    uart->rx_fifo_mask = (uint16_t)(uart->rx_fifo_size - 1); /* 计算 FIFO mask */
    uart->rx_head = 0;                                       /* 清空 FIFO 写指针 */
    uart->rx_tail = 0;                                       /* 清空 FIFO 读指针 */
    uart->rx_dma_pos = 0;                                    /* 清空 DMA 写指针 */
}

/* ============================================================
 *                     DMA 数据 → FIFO
 * ============================================================ */

/**
 * @brief 把 DMA 本次新增的数据搬进 FIFO。
 * @param uart     UART 对象
 * @param position DMA 当前写指针位置（= rx_dma_size时表示刚好绕回起点）
 */
void uart_dma_push(uart_desc_t uart, uint16_t position)
{
    uint16_t last = uart->rx_dma_pos;  /* 上次搬运的 DMA 写指针位置 */
    uint16_t size = uart->rx_dma_size; /* DMA 缓冲区大小 */

    if (position == size) /* DMA 写指针绕回起点 */
    {
        fifo_write(uart, &uart->rx_dma_buffer[last], (uint16_t)(size - last));
        position = 0;
    }
    else if (position > last) /* DMA 写指针在上次搬运位置之后 */
    {
        fifo_write(uart, &uart->rx_dma_buffer[last], (uint16_t)(position - last));
    }
    else if (position < last) /* DMA 写指针在上次搬运位置之前，说明 DMA 缓冲区已绕回起点 */
    {
        fifo_write(uart, &uart->rx_dma_buffer[last], (uint16_t)(size - last));
        fifo_write(uart, uart->rx_dma_buffer, position);
    }
    /* position == last: 无新数据 */

    uart->rx_dma_pos = position; /* 更新 DMA 写指针位置 */
}

/* ============================================================
 *                        中断处理
 * ============================================================ */

/**
 * @brief 处理 USART 空闲（IDLE）中断，把尚未触发 HT/TC 的数据搬进 FIFO。
 *        在 USARTx_IRQHandler 里调用。
 */
void uart_handle(uart_desc_t uart)
{
    volatile uint32_t tmp;

    if (USART_GetITStatus(uart->instance, USART_IT_IDLE) != RESET)
    {
        /* 读 SR + 读 DR 清除 IDLE 标志 */
        tmp = uart->instance->SR;
        tmp = uart->instance->DR;
        (void)tmp;

        /* DMA 已写位置 = 总大小 - 剩余计数 */
        uart_dma_push(uart, (uint16_t)(uart->rx_dma_size - DMA_GetCurrDataCounter(uart->dma_channel)));
    }
}

/* ============================================================
 *                          发送
 * ============================================================ */

/**
 * @brief 阻塞发送指定长度的数据。
 */
void uart_send(uart_desc_t uart, const uint8_t *data, uint16_t length)
{
    uint16_t i;

    for (i = 0; i < length; i++)
    {
        while (USART_GetFlagStatus(uart->instance, USART_FLAG_TXE) == RESET)
        {
        }
        USART_SendData(uart->instance, data[i]);
    }

    while (USART_GetFlagStatus(uart->instance, USART_FLAG_TC) == RESET)
    {
    }
}

/**
 * @brief 阻塞发送字符串。
 */
void uart_send_string(uart_desc_t uart, const char *string)
{
    uart_send(uart, (const uint8_t *)string, (uint16_t)strlen(string));
}

/* ============================================================
 *                          读取
 * ============================================================ */

/**
 * @brief 返回 FIFO 中当前可读字节数。
 */
uint16_t uart_available(const uart_desc_t uart)
{
    uint16_t head = uart->rx_head;
    uint16_t tail = uart->rx_tail;
    return (uint16_t)(((uint32_t)head - (uint32_t)tail) & uart->rx_fifo_mask);
}

/**
 * @brief 从 FIFO 读一段数据。
 * @return 实际读取的字节数。
 */
uint16_t uart_read(uart_desc_t uart, uint8_t *buf, uint16_t len)
{
    uint16_t available = uart_available(uart);
    uint16_t read_size = (len < available) ? len : available;

    fifo_data_out(uart, uart->rx_tail, buf, read_size);
    uart->rx_tail = fifo_new_index(uart, uart->rx_tail, read_size);
    return read_size;
}

/**
 * @brief 从 FIFO 读取一行（以 '\n' 结束，自动去掉行尾 '\r'）。
 * @return 实际复制的字节数；没有完整一行时返回 0。
 */
uint16_t uart_readline(uart_desc_t uart, uint8_t *line, uint16_t size)
{
    uint16_t available;
    uint16_t line_length = 0;
    uint16_t index;
    uint16_t data_length;
    uint16_t copy_length;

    if (size == 0)
    {
        return 0;
    }

    available = uart_available(uart);
    index = uart->rx_tail;
    while ((line_length < available) && (uart->rx_fifo_buffer[index] != '\n'))
    {
        line_length++;
        index = fifo_new_index(uart, index, 1);
    }

    if (line_length == available)
    {
        return 0; /* 还没有完整一行 */
    }

    data_length = line_length;
    if ((data_length > 0) &&
        (uart->rx_fifo_buffer[fifo_new_index(uart, uart->rx_tail,
                                             (uint16_t)(data_length - 1))] == '\r'))
    {
        data_length--;
    }

    copy_length = (data_length < (uint16_t)(size - 1))
                      ? data_length
                      : (uint16_t)(size - 1);
    fifo_data_out(uart, uart->rx_tail, line, copy_length);
    line[copy_length] = '\0';
    uart->rx_tail = fifo_new_index(uart, uart->rx_tail, (uint16_t)(line_length + 1));

    return copy_length;
}
