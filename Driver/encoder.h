#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f10x.h"

/* Rotary encoder A/B inputs: PE0 and PE1, active low with internal pull-ups. */
#define ENCODER_PORT GPIOE
#define ENCODER_A_PIN GPIO_Pin_0
#define ENCODER_B_PIN GPIO_Pin_1

void encoder_init(void);
int32_t encoder_get_value(void);
void encoder_set_value(int32_t value);

#endif /* __ENCODER_H */
