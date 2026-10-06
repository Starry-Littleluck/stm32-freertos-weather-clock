#include "stm32f10x.h"
#include "board.h"
#include "led.h"
#include "delay.h"
#include "usart1.h"
#include "key.h"
#include "timer.h"

static uint8_t key_report(key_desc_t key, const char *name)
{
	uint8_t single = 0U;

	if (key_check(key, KEY_DOWN))
		printf("%s down\r\n", name);
	if (key_check(key, KEY_RELEASE))
		printf("%s release\r\n", name);
	if (key_check(key, KEY_SINGLE)) {
		printf("%s single\r\n", name);
		single = 1U;
	}
	if (key_check(key, KEY_DOUBLE))
		printf("%s double\r\n", name);
	if (key_check(key, KEY_LONG))
		printf("%s long\r\n", name);
	if (key_check(key, KEY_REPEAT))
		printf("%s repeat\r\n", name);
	return single;
}

static uint8_t s_led0_on;
static uint8_t s_led1_on;

int main(void)
{
	board_lowlevel_init();
	delay_init();
	led_init();
	usart1_init(115200);
	key_init();
	timer_init();
	printf("key/timer test ready\r\n");

	while(1){
		if(usart1_available()>0){
			uint8_t buf[100];
			uint16_t len = usart1_readline(buf, sizeof(buf));
			if(len>0){
				printf("Received %d bytes:%.*s\r\n", len, len, buf);
			}
		}

		/* 单击 KEY0/KEY1 翻转对应 LED，验证按键、定时器和 LED 链路。 */
		if (key_report(KEY0, "KEY0")) {
			s_led0_on = (uint8_t)!s_led0_on;
			led_set(LED0, s_led0_on != 0U);
		}
		if (key_report(KEY1, "KEY1")) {
			s_led1_on = (uint8_t)!s_led1_on;
			led_set(LED1, s_led1_on != 0U);
		}
		key_report(KEY2, "KEY2");
		key_report(KEY_UP, "KEY_UP");
	}
}
