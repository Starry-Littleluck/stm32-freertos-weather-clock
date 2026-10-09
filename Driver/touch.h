#ifndef __TOUCH_H
#define __TOUCH_H

#include "stm32f10x.h"

/* ALIENTEK 4.3 inch capacitive panel, portrait orientation. */
#define TOUCH_WIDTH  480U
#define TOUCH_HEIGHT 800U

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint8_t pressed;
} touch_point_t;

/* 0: detected and initialized, non-zero: no supported controller found. */
uint8_t touch_init(void);
/* Poll the controller. The point is marked pressed when a contact is present. */
uint8_t touch_scan(touch_point_t *point);
uint8_t touch_is_ready(void);

#endif /* __TOUCH_H */
