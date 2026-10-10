#include "timer.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"
#include "misc.h"
#include "key.h"
#include "lvgl.h"

static volatile uint32_t s_millis;

uint32_t timer_millis(void)
{
    return s_millis;
}

void timer_init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    RCC_ClocksTypeDef clocks;
    uint32_t timer_clock;

    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if (clocks.PCLK1_Frequency != clocks.HCLK_Frequency)
        timer_clock *= 2U;

    /* TIM7 generates the LVGL time base independently of display rendering. */
    TIM_TimeBaseStructure.TIM_Period = 1000U - 1U;
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(timer_clock / 1000000U - 1U);
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;     /* 时钟分割 */
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; /* 向上计数模式 */
    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseStructure);             /* 初始化定时器 */

    /* 2. 配置中断 */
    NVIC_InitStructure.NVIC_IRQChannel = TIM7_IRQn;           /* 定时器7中断 */
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;        /* 子优先级 */
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           /* 使能中断 */
    NVIC_Init(&NVIC_InitStructure);

    /* 3. 使能定时器和中断 */
    TIM_ITConfig(TIM7, TIM_IT_Update, ENABLE); /* 开启更新中断 */
    TIM_Cmd(TIM7, ENABLE);                     /* 使能定时器 */
}

void TIM7_IRQHandler(void)
{
    static uint8_t key_elapsed_ms;

    if (TIM_GetITStatus(TIM7, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM7, TIM_IT_Update);
        s_millis++;
        lv_tick_inc(1U);
        if (++key_elapsed_ms >= 20U)
        {
            key_elapsed_ms = 0U;
            key_tick();
        }
    }
}
