#ifndef __LED__H
#define __LED__H

#include "stm32f10x.h"
#include "stdbool.h"
struct led_desc
{
    GPIO_TypeDef *Port;
    uint16_t Pin;
    BitAction OnBit;
    BitAction OffBit;
};

typedef struct led_desc* led_desc_t;

void led_init(led_desc_t led);
void led_set(led_desc_t led, bool onoff);
void led_on(led_desc_t led);
void led_off(led_desc_t led);

#endif /* __LED__H */
