#include "stm32f10x.h"
#include "board.h"
#include "led.h"
#include "delay.h"
#include "usart1.h"
int main(){
	board_lowlevel_init();
	delay_init();
	led_init();
	usart1_init(115200);
	while(1){
		if(usart1_available()>0){
			uint8_t buf[100];
			uint16_t len = usart1_readline(buf, sizeof(buf));
			if(len>0){
				printf("Received %d bytes:%.*s", len, len, buf);
			}
		}
	}
}
