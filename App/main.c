#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "board.h"
#include "led.h"
#include "delay.h"
#include "usart1.h"
#include "key.h"
#include "timer.h"
#include "lcd.h"
#include "encoder.h"
#include "at24c02.h"
#include "iic_hard.h"
#include "mpu6050.h"

static void lcd_show_mpu_data(const mpu6050_data_t *data)
{
	lcd_show_string(20U, 260U, LCD_WIDTH - 40U, 24U, "ACC X:", 16U);
	lcd_show_signed_num(80U, 260U, data->Acc_x, 8U, 16U);
	lcd_show_string(20U, 292U, LCD_WIDTH - 40U, 24U, "ACC Y:", 16U);
	lcd_show_signed_num(80U, 292U, data->Acc_y, 8U, 16U);
	lcd_show_string(20U, 324U, LCD_WIDTH - 40U, 24U, "ACC Z:", 16U);
	lcd_show_signed_num(80U, 324U, data->Acc_z, 8U, 16U);
	lcd_show_string(20U, 356U, LCD_WIDTH - 40U, 24U, "TEMP:", 16U);
	lcd_show_signed_num(80U, 356U, data->Temperature, 8U, 16U);
	lcd_show_string(20U, 388U, LCD_WIDTH - 40U, 24U, "GYRO X:", 16U);
	lcd_show_signed_num(80U, 388U, data->Gyro_x, 8U, 16U);
	lcd_show_string(20U, 420U, LCD_WIDTH - 40U, 24U, "GYRO Y:", 16U);
	lcd_show_signed_num(80U, 420U, data->Gyro_y, 8U, 16U);
	lcd_show_string(20U, 452U, LCD_WIDTH - 40U, 24U, "GYRO Z:", 16U);
	lcd_show_signed_num(80U, 452U, data->Gyro_z, 8U, 16U);
}

static uint8_t mpu6050_id_valid(uint8_t id)
{
	return (id == 0x68U || id == 0x70U || id == 0x71U || id == 0x73U) ? 1U : 0U;
}

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
	uint8_t at24_status;
	uint8_t at24_init_status;
	uint8_t at24_check_status = 1U;
	uint8_t mpu_status;
	uint8_t mpu_data_status = 1U;
	uint8_t mpu_id = 0U;
	uint8_t iic_debug_stage;
	uint16_t iic_debug_sr1;
	uint16_t iic_debug_sr2;
	uint16_t iic_debug_cr1;
	uint16_t iic_debug_cr2;
	mpu6050_data_t mpu_data = {0};

	board_lowlevel_init();
	delay_init();
	led_init();
	usart1_init(115200);
	key_init();
	encoder_init();
	encoder_set_value(50);
	timer_init();
	at24_init_status = at24c02_init();
	at24_status = at24_init_status;
	if (at24_init_status == 0U) {
		at24_check_status = at24c02_check();
		at24_status = at24_check_status;
	}
	mpu_status = mpu6050_init();
	if (mpu_status == 0U) {
		mpu_status = mpu6050_read_id(&mpu_id);
		if (mpu_status == 0U && mpu6050_id_valid(mpu_id) == 0U)
			mpu_status = 1U;
		if (mpu_status == 0U)
			mpu_data_status = mpu6050_read_data(&mpu_data);
	}
	lcd_init();
	lcd_set_text_color(LCD_BLUE);
	lcd_set_back_color(LCD_WHITE);
	lcd_show_string(20U, 20U, LCD_WIDTH - 40U, 32U, "STM32 WEATHER CLOCK", 16U);
	lcd_show_string(20U, 60U, LCD_WIDTH - 40U, 32U, "NT5510 480x800", 16U);
	lcd_show_string(20U, 100U, LCD_WIDTH - 40U, 32U, "BRIGHTNESS:", 16U);
	lcd_show_num(140U, 100U, 50U, 3U, 16U);
	lcd_show_string(164U, 100U, LCD_WIDTH - 164U, 32U, "%", 16U);
	lcd_show_string(20U, 140U, LCD_WIDTH - 40U, 24U, "AT24C02:", 16U);
	lcd_show_string(108U, 140U, LCD_WIDTH - 108U, 24U,
				 at24_status == 0U ? "OK" : "FAIL", 16U);
	lcd_show_string(20U, 180U, LCD_WIDTH - 40U, 24U, "MPU6050:", 16U);
	lcd_show_string(108U, 180U, LCD_WIDTH - 108U, 24U,
				 mpu_status == 0U ? "OK" : "FAIL", 16U);
	lcd_show_string(20U, 220U, LCD_WIDTH - 40U, 24U, "MPU ID:", 16U);
	lcd_show_num(100U, 220U, mpu_id, 3U, 16U);
	if (mpu_data_status == 0U)
		lcd_show_mpu_data(&mpu_data);
	printf("AT24 init=%u, check=%u, status=%u, MPU status=%u, ID=0x%02X, data=%u\r\n",
		   at24_init_status, at24_check_status, at24_status,
		   mpu_status, mpu_id, mpu_data_status);
	printf("I2C2 SR1=0x%04X SR2=0x%04X PB10=%u PB11=%u\r\n",
		   (unsigned int)I2C2->SR1, (unsigned int)I2C2->SR2,
		   (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_10),
		   (unsigned int)GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_11));
	iic_hard_get_debug(&iic_debug_stage, &iic_debug_sr1, &iic_debug_sr2,
					  &iic_debug_cr1, &iic_debug_cr2);
	printf("I2C2 debug stage=%u SR1=0x%04X SR2=0x%04X CR1=0x%04X CR2=0x%04X\r\n",
		   iic_debug_stage, (unsigned int)iic_debug_sr1,
		   (unsigned int)iic_debug_sr2, (unsigned int)iic_debug_cr1,
		   (unsigned int)iic_debug_cr2);
	printf("I2C2 MAPR=0x%08lX GPIOB CRH=0x%08lX IDR=0x%04X\r\n",
		   (unsigned long)AFIO->MAPR, (unsigned long)GPIOB->CRH,
		   (unsigned int)GPIOB->IDR);
	printf("key/timer/lcd test ready\r\n");

	while(1){
		static int32_t last_brightness = 50;
		static uint8_t sensor_divider;
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

		if (++sensor_divider >= 10U) {
			sensor_divider = 0U;
			if (mpu_status == 0U) {
				mpu_data_status = mpu6050_read_data(&mpu_data);
				if (mpu_data_status == 0U)
					lcd_show_mpu_data(&mpu_data);
			}
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
		delay_ms(10U);
	}
}
