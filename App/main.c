#include "font_download_demo.h"
#include "esp8266.h"
#include "lcd.h"
#include "usart1.h"
#include "delay.h"
#include <stdint.h>
#include <stdio.h>

static char s_uart1_command[96];
static uint8_t s_uart1_command_length;

static void main_update_weather(void)
{
    esp8266_weather_t weather;

    lcd_show_string(20U, 340U, LCD_WIDTH - 40U, 24U,
                    "WEATHER CONNECTING...", 16U);
    if (esp8266_weather_get(ESP8266_WEATHER_LOCATION, &weather,
                            20000UL) != 0U)
    {
        usart1_send_string("[WEATHER] HTTP GET FAILED: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
        lcd_show_string(20U, 340U, LCD_WIDTH - 40U, 24U,
                        "WEATHER REQUEST FAILED", 16U);
        return;
    }

    usart1_send_string("[WEATHER] ");
    usart1_send_string(weather.text);
    usart1_send_string(" ");
    usart1_send_string(weather.temperature);
    usart1_send_string(" C, code=");
    usart1_send_string(weather.code);
    usart1_send_string("\r\n");
    lcd_show_string(20U, 340U, LCD_WIDTH - 40U, 24U,
                    weather.text, 16U);
    lcd_show_string(20U, 364U, LCD_WIDTH - 40U, 24U,
                    "TEMP: ", 16U);
    lcd_show_string(72U, 364U, LCD_WIDTH - 72U, 24U,
                    weather.temperature, 16U);
    lcd_show_string(125U, 364U, LCD_WIDTH - 125U, 24U,
                    " C", 16U);
    lcd_show_string(20U, 388U, LCD_WIDTH - 40U, 24U,
                    "CODE: ", 16U);
    lcd_show_string(72U, 388U, LCD_WIDTH - 72U, 24U,
                    weather.code, 16U);
}

static void main_update_sntp(void)
{
    char text[64];
    esp8266_time_t time;

    if (esp8266_sntp_get_time(8, ESP8266_SNTP_SERVER, &time) != 0U)
    {
        usart1_send_string("[SNTP] FAILED: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
        lcd_show_string(20U, 316U, LCD_WIDTH - 40U, 24U,
                        "SNTP FAILED", 16U);
        return;
    }
    (void)snprintf(text, sizeof(text),
                   "[SNTP] %04u-%02u-%02u %02u:%02u:%02u\r\n",
                   time.year, time.month, time.day, time.hour,
                   time.minute, time.second);
    usart1_send_string(text);
    lcd_show_string(20U, 316U, LCD_WIDTH - 40U, 24U,
                    text, 16U);
}

static void main_show_status(const char *text)
{
    lcd_show_string(20U, 260U, LCD_WIDTH - 40U, 24U, text, 16U);
    usart1_send_string("[ESP8266] ");
    usart1_send_string(text);
    usart1_send_string("\r\n");
}

static uint8_t main_mqtt_connect(void)
{
    if (esp8266_mqtt_user_config(ESP8266_MQTT_DEVICE,
                                 ESP8266_MQTT_PRODUCT,
                                 ESP8266_MQTT_TOKEN) != 0U)
    {
        usart1_send_string("[ESP8266] MQTT USERCFG ERROR: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
        return 1U;
    }
    if (esp8266_mqtt_connect(ESP8266_MQTT_HOST, ESP8266_MQTT_PORT, 1U) != 0U)
    {
        usart1_send_string("[ESP8266] MQTT CONN ERROR: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
        return 1U;
    }
    if (esp8266_mqtt_subscribe(ESP8266_MQTT_SET_TOPIC, 0U) != 0U)
    {
        usart1_send_string("[ESP8266] MQTT SUB ERROR: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
    }
    return 0U;
}

static uint8_t main_connect_network(void)
{
    uint8_t retry;

    main_show_status("ESP8266 AT...");
    for (retry = 0U; retry < 2U; retry++)
    {
        if (esp8266_command("AT\r\n", "OK", 2000UL) == 0U)
        {
            break;
        }
    }
    if (retry == 2U)
    {
        main_show_status("AT FAILED");
        return 1U;
    }

    (void)esp8266_command("AT+MQTTCLEAN=0\r\n", "OK", 2000UL);
    main_show_status("WIFI CONNECTING...");
    for (retry = 0U; retry < 2U; retry++)
    {
        if (esp8266_connect_wifi(ESP8266_WIFI_SSID,
                                 ESP8266_WIFI_PASSWORD) == 0U)
        {
            break;
        }
        usart1_send_string("[ESP8266] WIFI ATTEMPT FAILED: ");
        usart1_send_string(esp8266_get_response());
        usart1_send_string("\r\n");
        if (retry == 0U)
        {
            main_show_status("WIFI RETRY...");
            delay_ms(500U);
        }
    }
    if (retry == 2U)
    {
        main_show_status("WIFI FAILED");
        return 1U;
    }

    /* 旧版 AT 固件的 HTTPS TCP 连接需要在 MQTT 占用链路前完成。 */
    main_update_sntp();
    main_update_weather();
    main_show_status("MQTT CONNECTING...");
    for (retry = 0U; retry < 2U; retry++)
    {
        if (main_mqtt_connect() == 0U)
        {
            break;
        }
        delay_ms(1000U);
    }
    if (retry == 2U)
    {
        main_show_status("MQTT FAILED");
        /* 天气接口只依赖 Wi-Fi，MQTT 失败不能阻止天气请求。 */
        return 0U;
    }
    main_show_status("WIFI MQTT READY");
    return 0U;
}

static void main_process_command(void)
{
    char command[sizeof(s_uart1_command) + 3U];
    const char *response;

    s_uart1_command[s_uart1_command_length] = '\0';
    if (s_uart1_command_length == 0U)
    {
        return;
    }

    (void)snprintf(command, sizeof(command), "%s\r\n", s_uart1_command);
    usart1_send_string("[AT TX] ");
    usart1_send_string(command);
    if (esp8266_command(command, "OK", 5000UL) == 0U)
    {
        response = esp8266_get_response();
        usart1_send_string("[AT RX] ");
        usart1_send_string(response);
        usart1_send_string("\r\n");
        lcd_show_string(20U, 292U, LCD_WIDTH - 40U, 24U,
                        "USART1 AT OK", 16U);
    }
    else
    {
        response = esp8266_get_response();
        usart1_send_string("[AT ERROR] ");
        usart1_send_string(response);
        usart1_send_string("\r\n");
        lcd_show_string(20U, 292U, LCD_WIDTH - 40U, 24U,
                        "USART1 AT ERROR", 16U);
    }
    s_uart1_command_length = 0U;
}

static void main_usart1_idle_byte(uint8_t data)
{
    if (data == (uint8_t)'\n')
    {
        main_process_command();
        return;
    }
    if (data == (uint8_t)'\r')
    {
        return;
    }
    if (s_uart1_command_length < sizeof(s_uart1_command) - 1U)
    {
        s_uart1_command[s_uart1_command_length++] = (char)data;
    }
}

static void main_process_esp8266_data(void)
{
    uint8_t data[128];
    uint16_t length;
    uint16_t index;
    uint16_t printable_length;

    while (esp8266_available() != 0U)
    {
        length = esp8266_read(data, sizeof(data) - 1U);
        if (length == 0U)
        {
            break;
        }
        printable_length = 0U;
        for (index = 0U; index < length; index++)
        {
            if (data[index] == '\r' || data[index] == '\n' ||
                (data[index] >= 0x20U && data[index] <= 0x7EU))
            {
                data[printable_length++] = data[index];
            }
            else
            {
                data[printable_length++] = '.';
            }
        }
        data[printable_length] = '\0';
        usart1_send_string("[ESP8266 RX] ");
        usart1_send_string((const char *)data);
        usart1_send_string("\r\n");
        lcd_show_string(20U, 292U, LCD_WIDTH - 40U, 24U,
                        (const char *)data, 16U);
    }
}

int main(void)
{

    font_download_demo_init();
    esp8266_init(115200U);
    /* ESP8266 上电启动和 AT 固件初始化需要一段时间。 */
    delay_ms(1500U);
    font_download_demo_set_idle_handler(main_usart1_idle_byte);
    (void)main_connect_network();

    while (1)
    {
        font_download_demo_run();
        main_process_esp8266_data();
    }
}
