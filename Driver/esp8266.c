#include "esp8266.h"
#include "delay.h"
#include "irq_priority.h"
#include "uart.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define ESP8266_RESPONSE_SIZE 768U

static uint8_t s_rx_fifo[ESP8266_RX_FIFO_SIZE];
static struct uart_desc s_uart_desc = {
    .Instance = USART2,
    .Gpio_port = GPIOA,
    .Pin_tx = GPIO_Pin_2,
    .Pin_rx = GPIO_Pin_3,
    .Dma_channel = NULL,
    .Rx_dma_buffer = NULL,
    .Rx_dma_size = 0U,
    .Rx_dma_pos = 0U,
    .Rx_fifo_buffer = s_rx_fifo,
    .Rx_fifo_size = sizeof(s_rx_fifo),
    .Rx_fifo_mask = 0U,
    .Rx_head = 0U,
    .Rx_tail = 0U,
    .Rx_line_scan = 0U,
    .Rx_line_length = 0U};
static uart_desc_t s_uart = &s_uart_desc;
static char s_response[ESP8266_RESPONSE_SIZE];
static char s_http_host[64];
static char s_http_request[384];
static char s_http_command[64];

/**
 * @brief 清空 ESP8266 接收缓冲区。
 */
static void esp8266_clear_rx(void)
{
    uint8_t data[32];

    while (uart_read(s_uart, data, sizeof(data)) != 0U)
    {
    }
}

/**
 * @brief 发送原始数据到 ESP8266。
 * @param data   数据缓冲区
 * @param length 数据长度
 */
static void esp8266_send_raw(const uint8_t *data, uint16_t length)
{
    uart_send(s_uart, data, length);
}

/**
 * @brief ESP8266 执行 AT 命令。
 * @param command    AT 命令字符串（以 \r\n 结尾）
 * @param expect     期望的响应字符串
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_command(const char *command, const char *expect, uint32_t timeout_ms)
{
    uint16_t used = 0U;
    uint32_t elapsed = 0UL;
    uint16_t command_length;
    uint8_t data;

    if (command == NULL || expect == NULL)
    {
        return 1U;
    }
    command_length = (uint16_t)strlen(command);
    if (command_length != 0U)
    {
        esp8266_clear_rx();
        esp8266_send_raw((const uint8_t *)command, command_length);
    }
    s_response[0] = '\0';

    while (elapsed < timeout_ms)
    {
        while (uart_read(s_uart, &data, 1U) != 0U)
        {
            if (used < (uint16_t)(sizeof(s_response) - 1U))
            {
                s_response[used++] = (char)data;
                s_response[used] = '\0';
                if (strstr(s_response, expect) != NULL)
                {
                    return 0U;
                }
                if (strstr(s_response, "ERROR") != NULL)
                {
                    return 1U;
                }
            }
        }
        delay_ms(1U);
        elapsed++;
    }
    return 1U;
}

const char *esp8266_get_response(void)
{
    return s_response;
}

/**
 * @brief 初始化 ESP8266 模块。
 * @param baudrate 串口波特率
 */
void esp8266_init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;
    volatile uint16_t status;

    uart_init(s_uart);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    gpio.GPIO_Pin = s_uart->Pin_tx;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(s_uart->Gpio_port, &gpio);

    gpio.GPIO_Pin = s_uart->Pin_rx;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(s_uart->Gpio_port, &gpio);

    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(s_uart->Instance, &usart);

    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = IRQ_PRIORITY_BACKGROUND;
    nvic.NVIC_IRQChannelSubPriority = 1U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(s_uart->Instance, ENABLE);
    status = s_uart->Instance->SR;
    status = s_uart->Instance->DR;
    (void)status;
    USART_ITConfig(s_uart->Instance, USART_IT_RXNE, ENABLE);
} 

/**
 * @brief ESP8266 连接 Wi-Fi。
 * @param ssid     Wi-Fi SSID
 * @param password Wi-Fi 密码
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_connect_wifi(const char *ssid, const char *password)
{
    char command[160];

    if (ssid == NULL || password == NULL || esp8266_command("AT+CWMODE=1\r\n", "OK", 2000UL) != 0U)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
    return esp8266_command(command, "OK", 15000UL);
}

static uint8_t esp8266_sntp_month(const char *name)
{
    static const char *const names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    uint8_t index;

    for (index = 0U; index < 12U; index++)
    {
        if (strcmp(name, names[index]) == 0)
        {
            return (uint8_t)(index + 1U);
        }
    }
    return 0U;
}

static uint8_t esp8266_sntp_parse_time(const char *response,
                                       esp8266_time_t *time)
{
    const char *line;
    char weekday[4];
    char month_name[4];
    unsigned int day;
    unsigned int hour;
    unsigned int minute;
    unsigned int second;
    unsigned int year;
    uint8_t month;

    if (response == NULL || time == NULL)
    {
        return 1U;
    }
    line = strstr(response, "+CIPSNTPTIME:");
    if (line == NULL ||
        sscanf(line, "+CIPSNTPTIME:%3s %3s %u %u:%u:%u %u",
               weekday, month_name, &day, &hour, &minute, &second,
               &year) != 7)
    {
        return 1U;
    }
    month = esp8266_sntp_month(month_name);
    if (month == 0U || day == 0U || day > 31U || hour > 23U ||
        minute > 59U || second > 59U || year > 65535U)
    {
        return 1U;
    }
    time->year = (uint16_t)year;
    time->month = month;
    time->day = (uint8_t)day;
    time->hour = (uint8_t)hour;
    time->minute = (uint8_t)minute;
    time->second = (uint8_t)second;
    return 0U;
}

/**
 * @brief 配置 SNTP 并获取已经同步的本地时间。
 * @param timezone 时区， 中国标准时间填写 8。
 * @param server   NTP 服务器域名。
 * @param time     输出的本地日期和时间。
 * @return 0 = 成功，1 = 配置失败、查询失败或尚未同步。
 */
uint8_t esp8266_sntp_get_time(int8_t timezone, const char *server,
                              esp8266_time_t *time)
{
    char command[128];
    uint8_t retry;

    if (server == NULL || time == NULL ||
        snprintf(command, sizeof(command),
                 "AT+CIPSNTPCFG=1,%d,\"%s\"\r\n", (int)timezone,
                 server) >= (int)sizeof(command))
    {
        return 1U;
    }
    if (esp8266_command(command, "OK", 3000UL) != 0U)
    {
        return 1U;
    }

    for (retry = 0U; retry < 5U; retry++)
    {
        delay_ms(1000U);
        if (esp8266_command("AT+CIPSNTPTIME?\r\n", "OK", 3000UL) == 0U &&
            esp8266_sntp_parse_time(esp8266_get_response(), time) == 0U &&
            time->year >= 2020U)
        {
            return 0U;
        }
    }
    return 1U;
}

/**
 * @brief ESP8266 启动 TCP 连接。
 * @param host 服务器主机名或 IP 地址
 * @param port 服务器端口号
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_tcp_start(const char *host, uint16_t port)
{
    char command[128];

    if (host == NULL)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command),"AT+CIPSTART=\"TCP\",\"%s\",%u\r\n", host, port);
    return esp8266_command(command, "OK", 10000UL);
}

/**
 * @brief ESP8266 发送数据。
 * @param data   数据缓冲区
 * @param length 数据长度
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_send(const uint8_t *data, uint16_t length)
{
    char command[32];

    if (data == NULL && length != 0U)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command), "AT+CIPSEND=%u\r\n", length);
    if (esp8266_command(command, ">", 2000UL) != 0U)
    {
        return 1U;
    }
    esp8266_send_raw(data, length);
    return 0U;
}

static uint8_t esp8266_copy_response(char *response, uint16_t response_size)
{
    const char *module_response;
    size_t response_length;

    if (response == NULL || response_size == 0U)
    {
        return 0U;
    }
    module_response = esp8266_get_response();
    response_length = strlen(module_response);
    if (response_length >= response_size)
    {
        response_length = response_size - 1U;
    }
    memcpy(response, module_response, response_length);
    response[response_length] = '\0';
    return 0U;
}

static uint8_t esp8266_http_error(const char *stage)
{
    char prefix[32];
    size_t prefix_length;
    size_t response_length;

    if (stage == NULL)
    {
        stage = "UNKNOWN";
    }
    (void)snprintf(prefix, sizeof(prefix), "[HTTP %s] ", stage);
    prefix_length = strlen(prefix);
    response_length = strlen(s_response);
    if (response_length == 0U)
    {
        (void)snprintf(s_response, sizeof(s_response),
                       "[HTTP %s] no response", stage);
        return 1U;
    }
    if (prefix_length + response_length >= sizeof(s_response))
    {
        response_length = sizeof(s_response) - prefix_length - 1U;
    }
    memmove(s_response + prefix_length, s_response, response_length + 1U);
    memcpy(s_response, prefix, prefix_length);
    return 1U;
}

/**
 * @brief 使用旧版 ESP8266 AT 固件的 SSL TCP 命令发送 HTTPS GET。
 *
 * Ai-Thinker v2.2.0 等固件使用 SSL TCP 连接、CIPSEND 和普通
 * HTTP 请求。
 */
static uint8_t esp8266_https_get_legacy(const char *url, uint32_t timeout_ms)
{
    const char *host_start;
    const char *path_start;
    uint16_t host_length;
    uint16_t request_length;

    if (url == NULL || strncmp(url, "https://", 8U) != 0)
    {
        return esp8266_http_error("URL");
    }
    host_start = url + 8U;
    path_start = strchr(host_start, '/');
    if (path_start == NULL)
    {
        path_start = host_start + strlen(host_start);
    }
    host_length = (uint16_t)(path_start - host_start);
    if (host_length == 0U || host_length >= sizeof(s_http_host))
    {
        return esp8266_http_error("URL");
    }
    memcpy(s_http_host, host_start, host_length);
    s_http_host[host_length] = '\0';

    if (path_start[0] == '\0')
    {
        path_start = "/";
    }
    if (snprintf(s_http_request, sizeof(s_http_request),
                 "GET %s HTTP/1.1\r\nHost: %s\r\n"
                 "Connection: close\r\n\r\n",
                 path_start, s_http_host) >= (int)sizeof(s_http_request))
    {
        return esp8266_http_error("REQUEST");
    }
    request_length = (uint16_t)strlen(s_http_request);

    if (esp8266_command("AT+CIPMUX=0\r\n", "OK", 2000UL) != 0U)
    {
        return esp8266_http_error("CIPMUX");
    }
    if (esp8266_command("AT+CIPMODE=0\r\n", "OK", 2000UL) != 0U)
    {
        return esp8266_http_error("CIPMODE");
    }
    if (snprintf(s_http_command, sizeof(s_http_command),
                 "AT+CIPSTART=\"SSL\",\"%s\",443\r\n", s_http_host) >=
        (int)sizeof(s_http_command))
    {
        return esp8266_http_error("SSL START");
    }
    if (esp8266_command(s_http_command, "OK", 15000UL) != 0U)
    {
        /* 个别旧版固件使用独立的 CIPSTARTSSL 命令。 */
        if (snprintf(s_http_command, sizeof(s_http_command),
                     "AT+CIPSTARTSSL=\"%s\",443\r\n", s_http_host) >=
            (int)sizeof(s_http_command) ||
            esp8266_command(s_http_command, "OK", 15000UL) != 0U)
        {
            return esp8266_http_error("SSL START");
        }
    }
    if (snprintf(s_http_command, sizeof(s_http_command), "AT+CIPSEND=%u\r\n",
                 request_length) >= (int)sizeof(s_http_command) ||
        esp8266_command(s_http_command, ">", 3000UL) != 0U)
    {
        return esp8266_http_error("CIPSEND");
    }
    esp8266_send_raw((const uint8_t *)s_http_request, request_length);

    /* Connection: close 使服务器发送完响应后主动断开。 */
    if (esp8266_command("", "CLOSED", timeout_ms) != 0U &&
        strstr(s_response, "HTTP/") == NULL)
    {
        return esp8266_http_error("RECEIVE");
    }
    if (strstr(s_response, "HTTP/") == NULL)
    {
        return esp8266_http_error("RECEIVE");
    }
    return 0U;
}

/**
 * @brief 使用旧版 SSL TCP 命令发送 HTTPS GET 请求。
 * @param url            完整 URL。
 * @param response       用于复制 AT 响应的缓冲区，可为 NULL。
 * @param response_size response 缓冲区大小。
 * @param timeout_ms     等待响应的超时时间。
 * @return 0 = 成功，1 = 失败。
 */
uint8_t esp8266_http_get(const char *url, char *response,
                         uint16_t response_size, uint32_t timeout_ms)
{
    if (url == NULL || (response == NULL && response_size != 0U))
    {
        return 1U;
    }
    if (esp8266_https_get_legacy(url, timeout_ms) != 0U)
    {
        return 1U;
    }
    (void)esp8266_copy_response(response, response_size);
    return 0U;
}

/**
 * @brief 从心知天气 JSON 响应中读取一个字符串字段。
 */
static uint8_t esp8266_json_value(const char *json, const char *name,
                                  char *value, uint16_t value_size)
{
    char key[48];
    const char *start;
    const char *end;
    uint16_t length;

    if (json == NULL || name == NULL || value == NULL || value_size == 0U)
    {
        return 1U;
    }
    if (snprintf(key, sizeof(key), "\"%s\"", name) >= (int)sizeof(key))
    {
        return 1U;
    }
    start = strstr(json, key);
    if (start == NULL)
    {
        return 1U;
    }
    start += strlen(key);
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    {
        start++;
    }
    if (*start != ':')
    {
        return 1U;
    }
    start++;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
    {
        start++;
    }
    if (*start != '\"')
    {
        return 1U;
    }
    start++;
    end = strchr(start, '\"');
    if (end == NULL)
    {
        return 1U;
    }
    length = (uint16_t)(end - start);
    if (length >= value_size)
    {
        length = value_size - 1U;
    }
    memcpy(value, start, length);
    value[length] = '\0';
    return 0U;
}

/**
 * @brief 请求心知天气实时天气并解析免费接口返回的三个字段。
 * @param location 城市拼音或心知天气支持的地点标识，例如 "beijing"。
 * @param weather  天气结果缓冲区。
 * @param timeout_ms HTTP 请求超时时间。
 * @return 0 = 成功，1 = 请求失败或 JSON 格式不符合预期。
 */
uint8_t esp8266_weather_get(const char *location, esp8266_weather_t *weather,
                            uint32_t timeout_ms)
{
    char url[256];

    if (location == NULL || weather == NULL)
    {
        return 1U;
    }
    if (snprintf(url, sizeof(url),
                 "https://api.seniverse.com/v3/weather/now.json?"
                 "key=%s&location=%s&language=zh-Hans&unit=c",
                 ESP8266_WEATHER_API_KEY, location) >= (int)sizeof(url))
    {
        return 1U;
    }
    if (esp8266_http_get(url, NULL, 0U, timeout_ms) != 0U)
    {
        return 1U;
    }
    if (esp8266_json_value(esp8266_get_response(), "text", weather->text,
                           sizeof(weather->text)) != 0U ||
        esp8266_json_value(esp8266_get_response(), "code", weather->code,
                           sizeof(weather->code)) != 0U ||
        esp8266_json_value(esp8266_get_response(), "temperature", weather->temperature,
                           sizeof(weather->temperature)) != 0U)
    {
        return 1U;
    }
    return 0U;
}

/**
 * @brief ESP8266 读取数据。
 * @param data   数据缓冲区
 * @param length 数据长度
 * @return 实际读取的字节数
 */
uint16_t esp8266_read(uint8_t *data, uint16_t length)
{
    return uart_read(s_uart, data, length);
}

/**
 * @brief ESP8266 可用数据长度。
 * @return 可用数据长度
 */
uint16_t esp8266_available(void)
{
    return uart_available(s_uart);
}

/**
 * @brief ESP8266 MQTT 用户配置。
 * @param client_id 客户端 ID
 * @param username  用户名
 * @param password  密码
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_user_config(const char *client_id,
                                 const char *username,
                                 const char *password)
{
    char command[256];

    if (client_id == NULL || username == NULL || password == NULL)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command),
                   "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",
                   client_id, username, password);
    return esp8266_command(command, "OK", 5000UL);
}

/**
 * @brief ESP8266 MQTT 连接。
 * @param host      MQTT 服务器主机名或 IP 地址
 * @param port      MQTT 服务器端口号
 * @param reconnect 是否自动重连（0 = 不重连，1 = 重连）
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_connect(const char *host, uint16_t port,
                             uint8_t reconnect)
{
    char command[128];

    if (host == NULL)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command),
                   "AT+MQTTCONN=0,\"%s\",%u,%u\r\n",
                   host, port, reconnect);
    return esp8266_command(command, "+MQTTCONNECTED", 10000UL);
}

/**
 * @brief ESP8266 MQTT 发布消息。
 * @param topic 主题
 * @param data  数据
 * @param qos   QoS 等级
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_publish(const char *topic, const char *data,
                             uint8_t qos)
{
    char command[128];
    uint16_t length;

    if (topic == NULL || data == NULL)
    {
        return 1U;
    }
    length = (uint16_t)strlen(data);
    (void)snprintf(command, sizeof(command),
                   "AT+MQTTPUBRAW=0,\"%s\",%u,%u,0\r\n",
                   topic, length, qos);
    if (esp8266_command(command, ">", 3000UL) != 0U)
    {
        return 1U;
    }
    esp8266_send_raw((const uint8_t *)data, length);
    return esp8266_command("", "+MQTTPUB:OK", 5000UL);
}

/**
 * @brief ESP8266 MQTT 订阅主题。
 * @param topic 主题
 * @param qos   QoS 等级
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_subscribe(const char *topic, uint8_t qos)
{
    char command[128];

    if (topic == NULL)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command),
                   "AT+MQTTSUB=0,\"%s\",%u\r\n", topic, qos);
    return esp8266_command(command, "OK", 2000UL);
}

/**
 * @brief ESP8266 MQTT 取消订阅主题。
 * @param topic 主题
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_unsubscribe(const char *topic)
{
    char command[128];

    if (topic == NULL)
    {
        return 1U;
    }
    (void)snprintf(command, sizeof(command),
                   "AT+MQTTUNSUB=0,\"%s\"\r\n", topic);
    return esp8266_command(command, "OK", 2000UL);
}

/**
 * @brief ESP8266 MQTT 断开连接。
 * @return 0 = 成功，1 = 失败
 */
uint8_t esp8266_mqtt_disconnect(void)
{
    return esp8266_command("AT+MQTTCLEAN=0\r\n", "OK", 2000UL);
}

/**
 * @brief USART2 中断处理函数。
 */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(s_uart->Instance, USART_IT_RXNE) != RESET)
    {
        uart_receive_byte(s_uart, (uint8_t)USART_ReceiveData(s_uart->Instance)); /* 读取数据寄存器清除中断标志 */
    }
}
