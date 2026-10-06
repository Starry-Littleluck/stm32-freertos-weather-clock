#include "key.h"
#include <stddef.h>

/* ---------- 时间参数（单位 ms；key_tick 每 20ms 调一次） ---------- */
#define TICK_MS 20U
#define DOUBLE_MS 200U /* 双击窗口 */
#define LONG_MS 2000U  /* 长按阈值 */
#define REPEAT_MS 100U /* 长按后连发周期 */

#define DOUBLE_TICKS (DOUBLE_MS / TICK_MS) /* 10  */
#define LONG_TICKS (LONG_MS / TICK_MS)     /* 100 */
#define REPEAT_TICKS (REPEAT_MS / TICK_MS) /* 5   */

/* 状态机 */
enum
{
    ST_IDLE = 0, /* 空闲（松开） */
    ST_PRESS,    /* 第一次按下：等长按或松开 */
    ST_WAIT,     /* 已松开：双击窗口内等第二次按下 */
    ST_SECOND,   /* 第二次按下（等待松开 → 双击） */
    ST_LPRESS    /* 已触发长按：连发中 */
};

/* ---------- 按键对象实例 ---------- */
/* KEY0/KEY1/KEY2：GPIOE，上拉，按下为低；KEY_UP：GPIOA，下拉，按下为高 */
struct key_desc g_key0 = {GPIOE, GPIO_Pin_4, Bit_RESET};
struct key_desc g_key1 = {GPIOE, GPIO_Pin_3, Bit_RESET};
struct key_desc g_key2 = {GPIOE, GPIO_Pin_2, Bit_RESET};
struct key_desc g_key_up = {GPIOA, GPIO_Pin_0, Bit_SET};

/* ============================================================
 *                     内部方法（private）
 * ============================================================ */

/**
 * @brief 读取按键的按下状态（已归一化：1=按下，0=松开）。
 */
static uint8_t key_read_level(key_desc_t key)
{
    BitAction level = (GPIO_ReadInputDataBit(key->Port, key->Pin) == Bit_SET)? Bit_SET: Bit_RESET;
    return (level == key->PressLevel) ? 1U : 0U;
}

/**
 * @brief 推进一个按键的边沿检测、计时器和状态机。
 */
static void key_update(key_desc_t key)
{
    uint8_t cur = key_read_level(key);

    /* ---- 边沿检测（20ms 采样，直接视为稳定状态） ---- */
    if (cur != key->Last)
    {
        key->Last = cur;
        key->Events |= cur ? KEY_DOWN : KEY_RELEASE;
    }

    /* ---- HOLD 是电平事件 ---- */
    if (key->Last)
        key->Events |= KEY_HOLD;
    else
        key->Events &= (uint8_t)~KEY_HOLD;

    /* ---- 计时器 ---- */
    if (key->Count)key->Count--;

    /* ---- 状态机 ---- */
    switch (key->State)
    {
    case ST_IDLE:
        if (key->Last)
        {
            key->State = ST_PRESS;
            key->Count = LONG_TICKS;
        }
        break;

    case ST_PRESS:
        if (!key->Last)
        {
            key->State = ST_WAIT;
            key->Count = DOUBLE_TICKS;
        }
        else if (!key->Count)
        {
            key->Events |= KEY_LONG;
            key->State = ST_LPRESS;
            key->Count = REPEAT_TICKS;
        }
        break;

    case ST_WAIT:
        if (key->Last)
        {
            key->Events |= KEY_DOUBLE;
            key->State = ST_SECOND;
        }
        else if (!key->Count)
        {
            key->Events |= KEY_SINGLE;
            key->State = ST_IDLE;
        }
        break;

    case ST_SECOND:
        if (!key->Last)
            key->State = ST_IDLE;
        break;

    case ST_LPRESS:
        if (!key->Last)
            key->State = ST_IDLE;
        else if (!key->Count)
        {
            key->Events |= KEY_REPEAT;
            key->Count = REPEAT_TICKS;
        }
        break;
    }
}

/* ============================================================
 *                     对外接口（public）
 * ============================================================ */

void key_init(void)
{
    GPIO_InitTypeDef gpio;
    key_desc_t keys[4] = {KEY0, KEY1, KEY2, KEY_UP};
    uint8_t i;

    for (i = 0U; i < 4U; i++)
    {
        gpio.GPIO_Pin = keys[i]->Pin;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        gpio.GPIO_Mode = (keys[i]->PressLevel == Bit_RESET) ? GPIO_Mode_IPU : GPIO_Mode_IPD;
        GPIO_Init(keys[i]->Port, &gpio);
        keys[i]->Last = key_read_level(keys[i]);
        keys[i]->State = ST_IDLE;
        keys[i]->Count = 0U;
        keys[i]->Events = 0U;
    }
}

void key_tick(void)
{
    key_update(KEY0);
    key_update(KEY1);
    key_update(KEY2);
    key_update(KEY_UP);
}

uint8_t key_get_state(key_desc_t key)
{
    return (key != NULL) ? key->Last : 0U;
}

uint8_t key_check(key_desc_t key, key_event_t event)
{
    uint8_t hit;
    uint32_t primask;

    if (key == NULL || event == KEY_EVENT_NONE)
        return 0U;

    primask = __get_PRIMASK();
    __disable_irq();

    hit = key->Events & (uint8_t)event;
    /* KEY_HOLD 是电平事件，不参与消费 */
    key->Events &= (uint8_t)~(event & (uint8_t)~KEY_HOLD);

    __set_PRIMASK(primask);
    return hit ? 1U : 0U;
}
