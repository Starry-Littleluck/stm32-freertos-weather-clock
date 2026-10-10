#include "encoder.h"

#include "irq_priority.h"
#include "misc.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_gpio.h"

static volatile int32_t s_encoder_value;
static volatile int8_t s_transition_accumulator;
static volatile uint8_t s_encoder_state;

/* Index: previous AB state in the high two bits, current state in the low two. */
static const int8_t s_transition_table[16] =
{
    0, -1,  1,  0,
    1,  0,  0, -1,
   -1,  0,  0,  1,
    0,  1, -1,  0
};

static uint8_t encoder_read_state(void)
{
    uint8_t state = 0U;

    if (GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_A_PIN) != Bit_RESET)
    {
        state |= 0x02U;
    }
    if (GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_B_PIN) != Bit_RESET)
    {
        state |= 0x01U;
    }
    return state;
}

static void encoder_process_edge(void)
{
    uint8_t current_state = encoder_read_state();
    uint8_t transition = (uint8_t)((s_encoder_state << 2) | current_state);
    int8_t step = s_transition_table[transition];

    s_encoder_state = current_state;
    if (step == 0)
    {
        return;
    }

    s_transition_accumulator += step;
    if (s_transition_accumulator >= 4)
    {
        s_encoder_value++;
        s_transition_accumulator = 0;
    }
    else if (s_transition_accumulator <= -4)
    {
        s_encoder_value--;
        s_transition_accumulator = 0;
    }
}

void encoder_init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    gpio.GPIO_Pin = ENCODER_A_PIN | ENCODER_B_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(ENCODER_PORT, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource0);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource1);

    s_encoder_value = 0;
    s_transition_accumulator = 0;
    s_encoder_state = encoder_read_state();

    EXTI_ClearITPendingBit(EXTI_Line0 | EXTI_Line1);
    exti.EXTI_Line = EXTI_Line0 | EXTI_Line1;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = EXTI0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = IRQ_PRIORITY_LOW_LATENCY;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    nvic.NVIC_IRQChannel = EXTI1_IRQn;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    NVIC_Init(&nvic);
}

int32_t encoder_get_value(void)
{
    int32_t value;
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    value = s_encoder_value;
    __set_PRIMASK(primask);
    return value;
}

void encoder_set_value(int32_t value)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    s_encoder_value = value;
    s_transition_accumulator = 0;
    __set_PRIMASK(primask);
}

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line0);
        encoder_process_edge();
    }
}

void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line1);
        encoder_process_edge();
    }
}
