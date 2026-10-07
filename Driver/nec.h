#ifndef __NEC_H
#define __NEC_H

#include "stm32f10x.h"

typedef enum
{
    NEC_EVENT_NONE = 0U,
    NEC_EVENT_FRAME = 1U,
    NEC_EVENT_REPEAT = 2U
} nec_event_t;

struct nec_desc
{
    GPIO_TypeDef *Port;
    uint16_t Pin;
    TIM_TypeDef *Timer;
    volatile uint8_t Address;
    volatile uint8_t Command;
    volatile uint8_t Valid;
    volatile uint8_t Event;
    volatile uint8_t State;
    volatile uint8_t Bit_count;
    volatile uint16_t Last_capture;
    volatile uint32_t Data;
};

typedef struct nec_desc *nec_desc_t;

extern struct nec_desc g_nec;

#define NEC1 (&g_nec)

void nec_init(void);
nec_event_t nec_read(uint8_t *address, uint8_t *command);
const char *nec_key_name(uint8_t command);
void nec_irq_handler(nec_desc_t nec);

#endif /* __NEC_H */
