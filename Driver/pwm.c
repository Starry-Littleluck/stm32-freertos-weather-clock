#include "pwm.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

void pwm_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_OCInitTypeDef channel;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    /* TIM3_CH3 is PB0 in the default (no-remap) configuration. */
    GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, DISABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, DISABLE);

    gpio.GPIO_Pin = GPIO_Pin_0;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &gpio);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = 72U - 1U;
    timer.TIM_Period = PWM_PERIOD - 1U;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &timer);

    TIM_OCStructInit(&channel);
    channel.TIM_OCMode = TIM_OCMode_PWM1;
    channel.TIM_OutputState = TIM_OutputState_Enable;
    channel.TIM_OCPolarity = TIM_OCPolarity_High;
    channel.TIM_Pulse = 0U;
    TIM_OC3Init(TIM3, &channel);
    TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

void pwm_set_compare(uint16_t compare)
{
    if (compare >= PWM_PERIOD)
    {
        compare = PWM_PERIOD - 1U;
    }
    TIM_SetCompare3(TIM3, compare);
}

void pwm_set_percent(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }
    pwm_set_compare((uint16_t)(((uint32_t)(PWM_PERIOD - 1U) * percent) / 100U));
}

uint16_t pwm_get_compare(void)
{
    return TIM3->CCR3;
}
