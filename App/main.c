#include "stm32f10x.h"
#include "board.h"
#include "led.h"
#include "delay.h"
int main(){
	board_lowlevel_init();
	delay_init();
	led_init(&led0);
	led_init(&led1);
	while(1){
		led_on(&led0);
		delay_ms(500);
		led_off(&led0);
		led_on(&led1);
		delay_ms(500);
		led_off(&led1);
	}
}
