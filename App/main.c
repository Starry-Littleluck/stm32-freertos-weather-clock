#include "stm32f10x.h"
#include "board.h"

int main(){
	board_lowlevel_init();
	led_init(&led0);
	led_init(&led1);
	led_on(&led0);
	while(1){
		
	}
}
