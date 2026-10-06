#include "led.h"
#include "stdio.h"

struct led_desc led0 = {GPIOB, GPIO_Pin_5, Bit_RESET, Bit_SET};
struct led_desc led1 = {GPIOE, GPIO_Pin_5, Bit_RESET, Bit_SET};

static void led_lowlevel_init(led_desc_t led)
{
    if (led == NULL)
    {
        return; /* 如果传入的LED描述结构体为空，则直接返回 */
    }
    GPIO_InitTypeDef GPIO_InitStructure;             /* GPIO初始化结构体 */
    GPIO_InitStructure.GPIO_Pin = led->Pin;          /* 选择要初始化的GPIO引脚 */
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; /* 设置为推挽输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; /* 设置GPIO速度为2MHz */
    GPIO_Init(led->Port, &GPIO_InitStructure);       /* 初始化GPIO */
    led_off(led);                                    /* 初始化时默认关闭LED */
}

void led_init(void)
{
    led_lowlevel_init(LED0);
    led_lowlevel_init(LED1);
}

void led_set(led_desc_t led, bool onoff)
{
    GPIO_WriteBit(led->Port, led->Pin, onoff ? led->OnBit : led->OffBit);
}

void led_on(led_desc_t led)
{
    GPIO_WriteBit(led->Port, led->Pin, led->OnBit);
}

void led_off(led_desc_t led)
{
    GPIO_WriteBit(led->Port, led->Pin, led->OffBit);
}
