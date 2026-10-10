#include "timer.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "misc.h"
#include "key.h"
#include "irq_priority.h"
void timer_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    /* 1. 配置定时器 TIM7 */
    TIM_TimeBaseStructure.TIM_Period = 20000 - 1;               /* 自动重装载寄存器周期的值 */
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;               /* 时钟预分频数 */
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     /* 时钟分割 */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* 向上计数模式 */
    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseStructure);             /* 初始化定时器 */

    /* 2. 配置中断 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;           /* 定时器7中断 */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = IRQ_PRIORITY_BACKGROUND; /* 可安全使用 FreeRTOS FromISR API */
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;        /* PriorityGroup_4 下不使用子优先级 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           /* 使能中断 */
    NVIC_Init(&NVIC_InitStructure);

    /* 3. 使能定时器和中断 */
    TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE); /* 开启更新中断 */
    TIM_Cmd(TIM7, ENABLE);                     /* 使能定时器 */
}

void TIM7_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM7, TIM_IT_Update); /* 清除中断标志位 */
        key_tick();                                 /* 调用按键扫描函数 */
    }
}
