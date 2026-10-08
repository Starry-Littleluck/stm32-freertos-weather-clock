/**
 * @file usart1.c
 * @author starry_littlelucky
 * @brief USART1 设备层实现：创建 uart1 对象、配置硬件、转接中断。
 */

#include "usart1.h"
#include "uart.h"

/*
 * 下载期间 Flash 擦除/写入会暂时阻塞主循环；使用更大的缓冲区，
 * 避免 DMA 环形缓冲回绕或软件 FIFO 满导致数据块错位。
 */
static uint8_t s_usart1_dma_buf[1024];
static uint8_t s_usart1_fifo_buf[4096];

static struct uart_desc s_usart1_desc = {
    .Instance = USART1,
    .Gpio_port = GPIOA,
    .Pin_tx = GPIO_Pin_9,
    .Pin_rx = GPIO_Pin_10,
    .Dma_channel = DMA1_Channel5,
    .Rx_dma_buffer = s_usart1_dma_buf,
    .Rx_dma_size = sizeof(s_usart1_dma_buf),
    .Rx_fifo_buffer = s_usart1_fifo_buf,
    .Rx_fifo_size = sizeof(s_usart1_fifo_buf)};
static uart_desc_t s_usart1 = &s_usart1_desc; /* 句柄 */

void usart1_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;   /* GPIO 配置结构体 */
    USART_InitTypeDef usart; /* USART 配置结构体 */
    DMA_InitTypeDef dma;     /* DMA 配置结构体 */
    NVIC_InitTypeDef nvic;   /* NVIC 配置结构体 */
    volatile uint32_t tmp;   /* 用于清标志位的临时变量 */

    /* ---------- 1. 让通用层清零软件状态（mask / head / tail / pos） ---------- */
    uart_init(s_usart1);

    /* ---------- 2. GPIO ---------- */
    /* TX：复用推挽输出 */
    gpio.GPIO_Pin = s_usart1->Pin_tx;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(s_usart1->Gpio_port, &gpio);

    /* RX：浮空输入 */
    gpio.GPIO_Pin = s_usart1->Pin_rx;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(s_usart1->Gpio_port, &gpio);

    /* ---------- 3. USART ---------- */
    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(s_usart1->Instance, &usart);

    /* ---------- 4. DMA ---------- */
    DMA_DeInit(s_usart1->Dma_channel);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&s_usart1->Instance->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)s_usart1->Rx_dma_buffer;
    dma.DMA_DIR = DMA_DIR_PeripheralSRC; /* 外设 → 内存 */
    dma.DMA_BufferSize = s_usart1->Rx_dma_size;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Circular; /* 循环模式 */
    dma.DMA_Priority = DMA_Priority_Low;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(s_usart1->Dma_channel, &dma);

    /* ---------- 5. NVIC ---------- */
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

    /* ---------- 6. 使能 ---------- */
    DMA_ITConfig(s_usart1->Dma_channel, DMA_IT_HT | DMA_IT_TC, ENABLE); /* 使能半传输和传输完成中断 */
    USART_Cmd(s_usart1->Instance, ENABLE);                              /* 使能 USART */
    DMA_Cmd(s_usart1->Dma_channel, ENABLE);                             /* 使能 DMA */
    USART_DMACmd(s_usart1->Instance, USART_DMAReq_Rx, ENABLE);          /* 使能 USART 接收 DMA */

    /* ---------- 7. 清标志位后开 IDLE 中断 ---------- */
    tmp = s_usart1->Instance->SR;
    tmp = s_usart1->Instance->DR;
    (void)tmp;
    USART_ITConfig(s_usart1->Instance, USART_IT_IDLE, ENABLE); /* 使能 IDLE 中断 */
}

void usart1_send(const uint8_t *data, uint16_t length)
{
    uart_send(s_usart1, data, length);
}

void usart1_send_string(const char *string)
{
    uart_send_string(s_usart1, string);
}

void usart1_poll(void)
{
    uart_dma_push(s_usart1);
}

uint16_t usart1_available(void)
{
    /* 查询前主动同步一次，覆盖短包未触发 IDLE/ DMA 中断的情况。 */
    uart_dma_push(s_usart1);
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
    uart_handle(s_usart1); /* IDLE 中断里调用，搬运 DMA 数据到 FIFO */
}

void DMA1_Channel5_IRQHandler(void)
{
    uint8_t sync = 0;

    if (DMA_GetITStatus(DMA1_IT_HT5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_HT5);
        sync = 1;
    }

    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        sync = 1;
    }

    if (sync != 0)
    {
        uart_dma_push(s_usart1); /* 合并处理 DMA 新数据 */
    }
}
