#ifndef __BOARD__H
#define __BOARD__H

#include "led.h"

void board_lowlevel_init(void);/* 初始化底层硬件 */

extern struct led_desc led0,led1;/* LED0和LED1的描述结构体 */

#endif
