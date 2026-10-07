#include "stm32f10x.h"
#include "board.h"
#include "led.h"
#include "delay.h"
#include "usart1.h"
#include "key.h"
#include "timer.h"
#include "lcd.h"
#include "encoder.h"

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
	encoder_init();
	encoder_set_value(50);
	timer_init();
	lcd_init();
	lcd_set_text_color(LCD_BLUE);
	lcd_set_back_color(LCD_WHITE);
	lcd_show_string(20U, 20U, LCD_WIDTH - 40U, 32U, "STM32 WEATHER CLOCK", 16U);
	lcd_show_string(20U, 60U, LCD_WIDTH - 40U, 32U, "NT5510 480x800", 16U);
	lcd_show_string(20U, 100U, LCD_WIDTH - 40U, 32U, "BRIGHTNESS:", 16U);
	lcd_show_num(140U, 100U, 50U, 3U, 16U);
	lcd_show_string(164U, 100U, LCD_WIDTH - 164U, 32U, "%", 16U);
	printf("key/timer/lcd test ready\r\n");

	while(1){
		static int32_t last_brightness = 50;
		int32_t brightness = encoder_get_value();

		if (brightness < 0) {
			brightness = 0;
			encoder_set_value(brightness);
		}
		else if (brightness > 100) {
			brightness = 100;
			encoder_set_value(brightness);
		}

		if (brightness != last_brightness) {
			last_brightness = brightness;
			lcd_set_backlight((uint8_t)brightness);
			lcd_show_num(140U, 100U, (uint32_t)brightness, 3U, 16U);
		}

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
