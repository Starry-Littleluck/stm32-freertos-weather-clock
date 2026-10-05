/**
 * @file usart1.c
 * @author starry_littlelucky
 * @brief USART1 设备层实现：创建 uart1 对象、配置硬件、转接中断。
 */

#include "usart1.h"
#include "uart.h"

static uint8_t s_usart1_dma_buf[512];   /* DMA 循环缓冲区，建议 2 的幂 */
static uint8_t s_usart1_fifo_buf[1024]; /* 软件 FIFO，容量必须 2 的幂  */

static struct uart_desc s_usart1_desc;        /* 对象实体，仅本文件可见      */
static uart_desc_t s_usart1 = &s_usart1_desc; /* 句柄 */

void usart1_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;   /* GPIO 配置结构体 */
    USART_InitTypeDef usart; /* USART 配置结构体 */
    DMA_InitTypeDef dma;     /* DMA 配置结构体 */
    NVIC_InitTypeDef nvic;   /* NVIC 配置结构体 */
    volatile uint32_t tmp;   /* 用于清标志位的临时变量 */

    /* ---------- 1. 填充对象（把"这个设备是谁"告诉通用层） ---------- */
    s_usart1->instance = USART1;    /* 使用USART1 */
    s_usart1->gpio_port = GPIOA;    /* 使用GPIOA */
    s_usart1->pin_tx = GPIO_Pin_9;  /* TX = PA9 */
    s_usart1->pin_rx = GPIO_Pin_10; /* RX = PA10 */

    s_usart1->dma_channel = DMA1_Channel5; /* RX = DMA1_Channel5 */

    s_usart1->rx_dma_buffer = s_usart1_dma_buf;       /* 用户提供，DMA 循环写入 */
    s_usart1->rx_dma_size = sizeof(s_usart1_dma_buf); /* 大小，建议 2 的幂 */

    s_usart1->rx_fifo_buffer = s_usart1_fifo_buf;       /* 用户提供，容量必须为 2 的幂 */
    s_usart1->rx_fifo_size = sizeof(s_usart1_fifo_buf); /* 容量 */

    /* ---------- 2. 让通用层清零软件状态（mask / head / tail / pos） ---------- */
    uart_init(s_usart1);

    /* ---------- 3. GPIO ---------- */
    /* TX：复用推挽输出 */
    gpio.GPIO_Pin = s_usart1->pin_tx;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(s_usart1->gpio_port, &gpio);

    /* RX：浮空输入 */
    gpio.GPIO_Pin = s_usart1->pin_rx;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(s_usart1->gpio_port, &gpio);

    /* ---------- 4. USART ---------- */
    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(s_usart1->instance, &usart);

    /* ---------- 5. DMA ---------- */
    DMA_DeInit(s_usart1->dma_channel);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&s_usart1->instance->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)s_usart1->rx_dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC; /* 外设 → 内存 */
    dma.DMA_BufferSize = s_usart1->rx_dma_size;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Circular; /* 循环模式 */
    dma.DMA_Priority = DMA_Priority_Low;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(s_usart1->dma_channel, &dma);

    /* ---------- 6. NVIC ---------- */
    /* DMA 中断优先级高于 USART 中断，保证数据搬运及时 */
    nvic.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2;
    nvic.NVIC_IRQChannelSubPriority = 0;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    /* ---------- 7. 使能 ---------- */
    DMA_ITConfig(s_usart1->dma_channel, DMA_IT_HT | DMA_IT_TC, ENABLE); /* 使能半传输和传输完成中断 */
    USART_Cmd(s_usart1->instance, ENABLE);                              /* 使能 USART */
    DMA_Cmd(s_usart1->dma_channel, ENABLE);                             /* 使能 DMA */
    USART_DMACmd(s_usart1->instance, USART_DMAReq_Rx, ENABLE);          /* 使能 USART 接收 DMA */

    /* ---------- 8. 清标志位后开 IDLE 中断 ---------- */
    tmp = s_usart1->instance->SR;
    tmp = s_usart1->instance->DR;
    (void)tmp;
    USART_ITConfig(s_usart1->instance, USART_IT_IDLE, ENABLE); /* 使能 IDLE 中断 */
}

void usart1_send(const uint8_t *data, uint16_t length)
{
    uart_send(s_usart1, data, length);
}

void usart1_send_string(const char *string)
{
    uart_send_string(s_usart1, string);
}

uint16_t usart1_available(void)
{
    return uart_available(s_usart1);
}

uint16_t usart1_read(uint8_t *buf, uint16_t len)
{
    return uart_read(s_usart1, buf, len);
}

uint16_t usart1_readline(uint8_t *line, uint16_t size)
{
    return uart_readline(s_usart1, line, size);
}

int fputc(int ch, FILE *f)
{
    usart1_send((const uint8_t *)&ch, 1);
    return ch;
}

void USART1_IRQHandler(void)
{
    uart_handle(s_usart1);/* IDLE 中断里调用，搬运 DMA 数据到 FIFO */
}

void DMA1_Channel5_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_HT5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_HT5);
        uart_dma_push(s_usart1, (uint16_t)(s_usart1->rx_dma_size / 2));/* 半传输中断里调用，将 DMA 数据搬运到 FIFO */
    }

    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        uart_dma_push(s_usart1, s_usart1->rx_dma_size);/* 传输完成中断里调用，将 DMA 数据搬运到 FIFO */
    }
}
