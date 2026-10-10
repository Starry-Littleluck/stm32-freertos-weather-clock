#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

static uint32_t cycles_per_us = 0U;

/* The project ships an older CMSIS core header without DWT and __get_IPSR(). */
#define DELAY_DEMCR_REG         (*((volatile uint32_t *)0xE000EDFCUL))
#define DELAY_DEMCR_TRCENA      (1UL << 24U)
#define DELAY_DWT_CTRL_REG      (*((volatile uint32_t *)0xE0001000UL))
#define DELAY_DWT_CYCCNT_REG    (*((volatile uint32_t *)0xE0001004UL))
#define DELAY_DWT_CYCCNTENA     (1UL << 0U)
#define DELAY_ICSR_REG          (*((volatile uint32_t *)0xE000ED04UL))
#define DELAY_ICSR_VECTACTIVE   (0x1FFUL)

static void delay_busy_cycles(uint32_t cycles)
{
    uint32_t start = DELAY_DWT_CYCCNT_REG;

    while ((uint32_t)(DELAY_DWT_CYCCNT_REG - start) < cycles)
    {
        __NOP();
    }
}

void delay_init(void)
{
    SystemCoreClockUpdate();
    cycles_per_us = SystemCoreClock / 1000000U;
    if (cycles_per_us == 0U)
    {
        cycles_per_us = 1U;
    }

    /* DWT is independent of SysTick, which is reserved for FreeRTOS. */
    DELAY_DEMCR_REG |= DELAY_DEMCR_TRCENA;
    DELAY_DWT_CYCCNT_REG = 0U;
    DELAY_DWT_CTRL_REG |= DELAY_DWT_CYCCNTENA;
}

void cpu_us(uint32_t us)
{
    uint32_t max_us;

    if (cycles_per_us == 0U)
    {
        delay_init();
    }

    max_us = 0xFFFFFFFFUL / cycles_per_us;
    while (us != 0U)
    {
        uint32_t chunk_us = us > max_us ? max_us : us;

        delay_busy_cycles(chunk_us * cycles_per_us);
        us -= chunk_us;
    }
}

void cpu_ms(uint32_t ms)
{
    while (ms-- != 0U)
    {
        cpu_us(1000U);
    }
}

void cpu_s(uint32_t s)
{
    while (s-- != 0U)
    {
        cpu_ms(1000U);
    }
}

void delay_us(uint32_t us)
{
    /* Microsecond timing cannot yield accurately on a 1 kHz FreeRTOS tick. */
    cpu_us(us);
}

void delay_ms(uint32_t ms)
{
    if (ms == 0U)
    {
        return;
    }

    if (((DELAY_ICSR_REG & DELAY_ICSR_VECTACTIVE) == 0U) &&
        (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        cpu_ms(ms);
    }
}

void delay_s(uint32_t s)
{
    while (s-- != 0U)
    {
        delay_ms(1000U);
    }
}
