#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"

#define ESP8266_RX_FIFO_SIZE 512U
#define ESP8266_WIFI_SSID       "CMCC-Zy7y"
#define ESP8266_WIFI_PASSWORD   "mfwt6efe"
#define ESP8266_MQTT_HOST       "mqtts.heclouds.com"
#define ESP8266_MQTT_PORT       1883U
#define ESP8266_MQTT_PRODUCT    "EgzXyE90Y0"
#define ESP8266_MQTT_DEVICE     "mytext"
#define ESP8266_MQTT_TOKEN      "version=2018-10-31&res=products%2FEgzXyE90Y0%2Fdevices%2Fmytext&et=1815735902&method=md5&sign=vWX2gYZNrO8mW2B1DFE4Og%3D%3D"
#define ESP8266_MQTT_SET_TOPIC  "$sys/"ESP8266_MQTT_PRODUCT "/"ESP8266_MQTT_DEVICE "/thing/property/set"
#define ESP8266_WEATHER_API_KEY "Sn7naGqnB43NqS7_L"
#define ESP8266_WEATHER_LOCATION "beijing"
#define ESP8266_SNTP_SERVER     "ntp1.aliyun.com"

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} esp8266_time_t;

typedef struct
{
    char text[32];
    char code[8];
    char temperature[8];
} esp8266_weather_t;

void esp8266_init(uint32_t baudrate);                                                                /* ESP8266 初始化 */
uint8_t esp8266_command(const char *command, const char *expect, uint32_t timeout_ms);               /* ESP8266 执行命令 */
const char *esp8266_get_response(void);                                                              /* 获取 ESP8266 响应 */
uint8_t esp8266_connect_wifi(const char *ssid, const char *password);                                /* ESP8266 连接 Wi-Fi */
uint8_t esp8266_sntp_get_time(int8_t timezone, const char *server,
                              esp8266_time_t *time);                                                  /* SNTP 获取时间 */
uint8_t esp8266_tcp_start(const char *host, uint16_t port);                                          /* ESP8266 TCP 连接 */
uint8_t esp8266_send(const uint8_t *data, uint16_t length);                                          /* ESP8266 发送数据 */
uint8_t esp8266_http_get(const char *url, char *response, uint16_t response_size,
                         uint32_t timeout_ms);                                                       /* HTTPS GET */
uint8_t esp8266_weather_get(const char *location, esp8266_weather_t *weather,
                            uint32_t timeout_ms);                                                    /* 心知天气 */
uint16_t esp8266_read(uint8_t *data, uint16_t length);                                               /* ESP8266 读取数据 */
uint16_t esp8266_available(void);                                                                    /* ESP8266 可读取数据长度 */
uint8_t esp8266_mqtt_user_config(const char *client_id, const char *username, const char *password); /* ESP8266 MQTT 用户配置 */
uint8_t esp8266_mqtt_connect(const char *host, uint16_t port, uint8_t reconnect);                    /* ESP8266 MQTT 连接 */
uint8_t esp8266_mqtt_publish(const char *topic, const char *data, uint8_t qos);                      /* ESP8266 MQTT 发布消息 */
uint8_t esp8266_mqtt_subscribe(const char *topic, uint8_t qos);                                      /* ESP8266 MQTT 订阅主题 */
uint8_t esp8266_mqtt_unsubscribe(const char *topic);                                                 /* ESP8266 MQTT 取消订阅主题 */
uint8_t esp8266_mqtt_disconnect(void);                                                               /* ESP8266 MQTT 断开连接 */

#endif /* __ESP8266_H */
