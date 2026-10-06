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
}; /* LED 描述结构体 */

typedef struct led_desc *led_desc_t; /* 句柄类型 */

extern struct led_desc led0, led1; /* LED0和LED1的描述结构体 */
#define LED0 (&led0)               /* LED0 句柄 */
#define LED1 (&led1)               /* LED1 句柄 */

void led_init(void);                      /* 初始化 LED（配置 GPIO） */
void led_set(led_desc_t led, bool onoff); /* 设置 LED 状态（onoff = true 打开，false 关闭） */
void led_on(led_desc_t led);              /* 打开 LED */
void led_off(led_desc_t led);             /* 关闭 LED */

#endif /* __LED__H */
