#include "delay.h"

static uint32_t fac_us = 0;
static uint32_t fac_ms = 0;
static uint32_t max_ms = 0; /* 最大单次可延时毫秒数 */

void delay_init(void)
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    fac_us = SystemCoreClock / 8000000; /* 9 */
    fac_ms = fac_us * 1000;             /* 9000 */
    max_ms = 0xFFFFFF / fac_ms;         /* 1864ms */
}

void delay_us(uint32_t us)
{
    uint32_t temp;
    /* 如果超过最大微秒数，可以分段或限制（这里做简单限制保护） */
    if (us > (0xFFFFFF / fac_us))
    {
        us = 0xFFFFFF / fac_us;
    }

    SysTick->LOAD = us * fac_us;              /* 设置重装载寄存器 */
    SysTick->VAL = 0x00;                      /* 清空计数器 */
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; /* 开始计数 */

    do
    {
        temp = SysTick->CTRL;
    } while ((temp & SysTick_CTRL_ENABLE_Msk) && (!(temp & SysTick_CTRL_COUNTFLAG_Msk)));

    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0x00;
}

void delay_ms(uint32_t ms)
{
    uint32_t temp;

    if (ms == 0)
        return;
    while (ms > 0)
    {
        uint32_t load_val;

        if (ms > max_ms)
        {
            load_val = max_ms * fac_ms;
            ms -= max_ms;
        }
        else
        {
            load_val = ms * fac_ms;
            ms = 0;
        }

        SysTick->LOAD = load_val;/* 设置重装载寄存器 */
        SysTick->VAL = 0x00;/* 清空计数器 */
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;/* 开始计数 */

        do
        {
            temp = SysTick->CTRL;
        } while ((temp & SysTick_CTRL_ENABLE_Msk) && (!(temp & SysTick_CTRL_COUNTFLAG_Msk)));

        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        SysTick->VAL = 0x00;
    }
}

void delay_s(uint32_t s)
{
    while (s--)
    {
        delay_ms(1000);
    }
}
