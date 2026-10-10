#ifndef IRQ_PRIORITY_H
#define IRQ_PRIORITY_H

#include "FreeRTOSConfig.h"

/*
 * NVIC priority grouping is configured as NVIC_PriorityGroup_4 in board.c.
 * Priorities at or below IRQ_PRIORITY_RTOS_SAFE may call FreeRTOS FromISR APIs.
 * IRQ_PRIORITY_LOW_LATENCY is reserved for short, time-critical ISRs that do
 * not call FreeRTOS APIs.
 */
#define IRQ_PRIORITY_LOW_LATENCY  ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY - 1U )
#define IRQ_PRIORITY_RTOS_SAFE    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY )
#define IRQ_PRIORITY_BACKGROUND   ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1U )

#endif /* IRQ_PRIORITY_H */
