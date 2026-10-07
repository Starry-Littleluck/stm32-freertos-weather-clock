#include "nec.h"

#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

enum
{
    NEC_STATE_IDLE = 0U,
    NEC_STATE_LEADER,
    NEC_STATE_DATA
};

#define NEC_LEADER_MIN_US 12000U
#define NEC_LEADER_MAX_US 15000U
#define NEC_REPEAT_MIN_US 10000U
#define NEC_REPEAT_MAX_US 12000U
#define NEC_BIT_ZERO_MIN_US 900U
#define NEC_BIT_ZERO_MAX_US 1400U
#define NEC_BIT_ONE_MIN_US 1900U
#define NEC_BIT_ONE_MAX_US 2600U

struct nec_desc g_nec =
{
    GPIOB,
    GPIO_Pin_9,
    TIM4,
    0U,
    0U,
    0U,
    NEC_EVENT_NONE,
    NEC_STATE_IDLE,
    0U,
    0U,
    0U
};

static void nec_reset_decoder(nec_desc_t nec)
{
    nec->State = NEC_STATE_IDLE;
    nec->Bit_count = 0U;
    nec->Data = 0U;
}

void nec_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef timer;
    TIM_ICInitTypeDef capture;
    NVIC_InitTypeDef nvic;

    gpio.GPIO_Pin = NEC1->Pin;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(NEC1->Port, &gpio);

    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = 72U - 1U;
    timer.TIM_Period = 0xFFFFU;
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(NEC1->Timer, &timer);

    TIM_ICStructInit(&capture);
    capture.TIM_Channel = TIM_Channel_4;
    capture.TIM_ICPolarity = TIM_ICPolarity_Falling;
    capture.TIM_ICSelection = TIM_ICSelection_DirectTI;
    capture.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    capture.TIM_ICFilter = 0x0FU;
    TIM_ICInit(NEC1->Timer, &capture);

    TIM_ClearITPendingBit(NEC1->Timer, TIM_IT_CC4);
    TIM_ITConfig(NEC1->Timer, TIM_IT_CC4, ENABLE);

    nvic.NVIC_IRQChannel = TIM4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2U;
    nvic.NVIC_IRQChannelSubPriority = 2U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    NEC1->Address = 0U;
    NEC1->Command = 0U;
    NEC1->Valid = 0U;
    NEC1->Event = NEC_EVENT_NONE;
    NEC1->Last_capture = 0U;
    nec_reset_decoder(NEC1);
    TIM_SetCounter(NEC1->Timer, 0U);
    TIM_Cmd(NEC1->Timer, ENABLE);
}

nec_event_t nec_read(uint8_t *address, uint8_t *command)
{
    uint32_t primask;
    nec_event_t event;

    primask = __get_PRIMASK();
    __disable_irq();
    event = (nec_event_t)NEC1->Event;
    if (event != NEC_EVENT_NONE)
    {
        if (address != 0)
            *address = NEC1->Address;
        if (command != 0)
            *command = NEC1->Command;
        NEC1->Event = NEC_EVENT_NONE;
    }
    __set_PRIMASK(primask);
    return event;
}

const char *nec_key_name(uint8_t command)
{
    switch (command)
    {
    case 0x45U: return "1";
    case 0x46U: return "2";
    case 0x47U: return "3";
    case 0x44U: return "4";
    case 0x40U: return "5";
    case 0x43U: return "6";
    case 0x07U: return "7";
    case 0x15U: return "8";
    case 0x09U: return "9";
    case 0x16U: return "*";
    case 0x19U: return "0";
    case 0x0DU: return "#";
    case 0x18U: return "UP";
    case 0x52U: return "DOWN";
    case 0x08U: return "LEFT";
    case 0x5AU: return "RIGHT";
    case 0x1CU: return "OK";
    default: return "UNKNOWN";
    }
}

void nec_irq_handler(nec_desc_t nec)
{
    uint16_t capture;
    uint16_t interval;
    uint8_t address;
    uint8_t address_inverse;
    uint8_t command;
    uint8_t command_inverse;

    if (nec == 0 || TIM_GetITStatus(nec->Timer, TIM_IT_CC4) == RESET)
        return;

    TIM_ClearITPendingBit(nec->Timer, TIM_IT_CC4);
    capture = TIM_GetCapture4(nec->Timer);

    if (nec->State == NEC_STATE_IDLE)
    {
        nec->Last_capture = capture;
        nec->State = NEC_STATE_LEADER;
        return;
    }

    interval = (uint16_t)(capture - nec->Last_capture);
    nec->Last_capture = capture;

    if (nec->State == NEC_STATE_LEADER)
    {
        if (interval >= NEC_LEADER_MIN_US && interval <= NEC_LEADER_MAX_US)
        {
            nec->State = NEC_STATE_DATA;
            nec->Bit_count = 0U;
            nec->Data = 0U;
            return;
        }

        if (interval >= NEC_REPEAT_MIN_US && interval <= NEC_REPEAT_MAX_US)
        {
            if (nec->Valid != 0U)
                nec->Event = NEC_EVENT_REPEAT;
            nec_reset_decoder(nec);
            return;
        }

        nec->State = NEC_STATE_LEADER;
        return;
    }

    if (interval >= NEC_BIT_ZERO_MIN_US && interval <= NEC_BIT_ZERO_MAX_US)
    {
        /* 逻辑 0，数据位已经默认为 0。 */
    }
    else if (interval >= NEC_BIT_ONE_MIN_US && interval <= NEC_BIT_ONE_MAX_US)
    {
        nec->Data |= (uint32_t)1U << nec->Bit_count;
    }
    else
    {
        nec_reset_decoder(nec);
        return;
    }

    nec->Bit_count++;
    if (nec->Bit_count < 32U)
        return;

    address = (uint8_t)nec->Data;
    address_inverse = (uint8_t)(nec->Data >> 8);
    command = (uint8_t)(nec->Data >> 16);
    command_inverse = (uint8_t)(nec->Data >> 24);
    nec_reset_decoder(nec);

    if ((uint8_t)~address != address_inverse ||
        (uint8_t)~command != command_inverse)
        return;

    nec->Address = address;
    nec->Command = command;
    nec->Valid = 1U;
    nec->Event = NEC_EVENT_FRAME;
}

void TIM4_IRQHandler(void)
{
    nec_irq_handler(NEC1);
}
