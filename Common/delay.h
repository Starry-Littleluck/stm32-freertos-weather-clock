#ifndef __DELAY__H
#define __DELAY__H

#include "stm32f10x.h"

void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);
void delay_s(uint32_t s);

/* Busy waits. These functions always occupy the CPU, even after the scheduler starts. */
void cpu_us(uint32_t us);
void cpu_ms(uint32_t ms);
void cpu_s(uint32_t s);

#endif /* __DELAY__H */
