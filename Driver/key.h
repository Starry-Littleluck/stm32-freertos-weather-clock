#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include <stdint.h>

/* 事件位掩码 */
typedef enum
{
    KEY_EVENT_NONE = 0x00,
    KEY_HOLD       = 0x01,   /* 电平：按下期间为 1，key_check 不清除它 */
    KEY_DOWN       = 0x02,   /* 边沿：按下瞬间 */
    KEY_RELEASE    = 0x04,   /* 边沿：松开瞬间 */
    KEY_SINGLE     = 0x08,   /* 单击 */
    KEY_DOUBLE     = 0x10,   /* 双击 */
    KEY_LONG       = 0x20,   /* 长按 */
    KEY_REPEAT     = 0x40    /* 长按后连发 */
} key_event_t;

/* 按键描述结构体 */
struct key_desc
{
    /* ---------- 硬件 ---------- */
    GPIO_TypeDef *Port;          /* GPIOA / GPIOB / ...                   */
    uint16_t      Pin;           /* 例如 GPIO_Pin_4                       */
    BitAction     PressLevel;    /* 按下时的有效电平：Bit_RESET(上拉) 或 Bit_SET(下拉) */

    /* ---------- 运行时（驱动内部使用，勿直接修改） ---------- */
    volatile uint8_t  Last;      /* 最近一次采样状态：1=按下，0=松开 */
    volatile uint8_t  State;     /* 状态机 */
    volatile uint16_t Count;     /* 状态机计时器（tick，1 tick = 20ms） */
    volatile uint8_t  Events;    /* 事件累积位掩码 */
};

typedef struct key_desc *key_desc_t;

/* 板载按键对象 */
extern struct key_desc g_key0;    /* PE4，上拉，按下为低 */
extern struct key_desc g_key1;    /* PE3，上拉，按下为低 */
extern struct key_desc g_key2;    /* PE2，上拉，按下为低 */
extern struct key_desc g_key_up;  /* PA0，下拉，按下为高 */

#define KEY0    (&g_key0)
#define KEY1    (&g_key1)
#define KEY2    (&g_key2)
#define KEY_UP  (&g_key_up)

void    key_init(void);                                 /* 初始化四个按键 GPIO */
void    key_tick(void);                                 /* 每 20ms 调用一次 */
uint8_t key_get_state(key_desc_t key);                  /* 1=按下，0=松开 */
uint8_t key_check(key_desc_t key, key_event_t event);   /* 查询并消费事件 */

#endif /* __KEY_H */
