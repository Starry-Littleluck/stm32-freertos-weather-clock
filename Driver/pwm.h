#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

#define PWM_PERIOD 500U

void pwm_init(void);
void pwm_set_compare(uint16_t compare);
void pwm_set_percent(uint8_t percent);
uint16_t pwm_get_compare(void);

#endif
