#include "board.h"

struct led_desc led0 = {GPIOB,GPIO_Pin_5,Bit_RESET,Bit_SET};
struct led_desc led1 = {GPIOE,GPIO_Pin_5,Bit_RESET,Bit_SET};


void board_lowlevel_init(void){
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);
}
