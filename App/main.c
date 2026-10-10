#include "board.h"
#include "delay.h"
#include "key.h"
#include "lcd.h"
#include "led.h"
#include "lvgl.h"
#include "lvgl_port.h"
#include "touch.h"
#include "timer.h"
#include "usart1.h"
#include "weather_clock_ui.h"
#include "weather_clock_settings.h"

#include <stdio.h>

int main(void)
{
    lv_display_t *display;
    uint32_t last_heartbeat_ms;
    uint8_t led_state = 0U;
    uint8_t first_refresh = 1U;
    lv_mem_monitor_t memory;
    char diagnostic[80];
    weather_clock_settings_t settings;

    board_lowlevel_init();
    delay_init();
    led_init();
    usart1_init(115200U);
    key_init();
    lcd_init();
    weather_clock_settings_load(&settings);
    lcd_set_backlight(settings.brightness);
    (void)touch_init();

    lv_init();
    timer_init();
    last_heartbeat_ms = timer_millis();
    display = lvgl_port_init();
    if (display != 0)
        weather_clock_ui_init();
    else
        usart1_send_string("LVGL display init failed\r\n");

    if (display != 0)
        usart1_send_string("Weather clock UI ready\r\n");
    if (lv_mem_test() != LV_RESULT_OK)
    {
        usart1_send_string("LVGL heap corrupted before first refresh\r\n");
        for (;;)
        {
            led_on(LED0);
        }
    }
    lv_mem_monitor(&memory);
    (void)snprintf(diagnostic, sizeof(diagnostic),
                   "LVGL before refresh: free=%lu, largest=%lu\r\n",
                   (unsigned long)memory.free_size,
                   (unsigned long)memory.free_biggest_size);
    usart1_send_string(diagnostic);
    for (;;)
    {
        lv_timer_handler();
        if (first_refresh != 0U)
        {
            first_refresh = 0U;
            if (lv_mem_test() != LV_RESULT_OK)
            {
                usart1_send_string("LVGL heap corrupted during first refresh\r\n");
                for (;;)
                    led_on(LED0);
            }
            usart1_send_string("LVGL first refresh completed\r\n");
        }
        weather_clock_ui_process();
        delay_ms(5U);

        if ((uint32_t)(timer_millis() - last_heartbeat_ms) >= 1000U)
        {
            last_heartbeat_ms = timer_millis();
            led_state ^= 1U;
            led_set(LED0, led_state != 0U);
        }
    }
}
