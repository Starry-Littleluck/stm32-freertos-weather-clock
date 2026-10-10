#include "esp8266.h"
#include "lcd.h"
#include "lvgl_port.h"
#include "timer.h"
#include "weather_clock_settings.h"
#include "weather_clock_font.h"
#include "weather_clock_ui.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef enum
{
    UI_PAGE_HOME = 0,
    UI_PAGE_WEATHER,
    UI_PAGE_SETTINGS,
    UI_PAGE_COUNT
} ui_page_t;

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} ui_clock_t;

#define UI_BG             0x101827U
#define UI_PANEL          0x1A2638U
#define UI_PANEL_ALT      0x22334AU
#define UI_ACCENT         0x41C7A5U
#define UI_ACCENT_BLUE    0x4BA3FFU
#define UI_TEXT           0xF2F6FCU
#define UI_MUTED          0x9AAAC0U
#define UI_WARNING        0xF7B955U

#define UI_MARGIN         16
#define UI_CONTENT_WIDTH  ((int32_t)LCD_WIDTH - UI_MARGIN * 2)
#define UI_NAV_Y          ((int32_t)LCD_HEIGHT - 88)

static ui_clock_t s_clock = {2026U, 2U, 2U, 22U, 6U, 38U};
static esp8266_weather_t s_weather = {"晴", "0", "27"};
static uint8_t s_auto_refresh = 1U;
static uint8_t s_wifi_online;
static uint8_t s_weather_stale;
static weather_clock_settings_t s_settings;

static lv_obj_t *s_pages[UI_PAGE_COUNT];
static lv_obj_t *s_navigation;
static ui_page_t s_current_page = UI_PAGE_COUNT;
static ui_page_t s_requested_page = UI_PAGE_COUNT;
static uint32_t s_last_clock_ms;
static lv_obj_t *s_clock_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_home_weather_label;
static lv_obj_t *s_home_temperature_label;
static lv_obj_t *s_home_status_label;
static lv_obj_t *s_weather_temperature_label;
static lv_obj_t *s_weather_summary_label;
static lv_obj_t *s_weather_code_label;
static lv_obj_t *s_weather_status_label;
static lv_obj_t *s_settings_brightness_label;
static lv_obj_t *s_settings_status_label;
static lv_obj_t *s_settings_city_label;
static lv_obj_t *s_home_city_label;
static lv_obj_t *s_weather_city_label;
static lv_obj_t *s_city_popup;
static lv_obj_t *s_city_textarea;

static const uint32_t s_page_colors[UI_PAGE_COUNT] =
{
    UI_ACCENT, UI_ACCENT_BLUE, UI_WARNING
};

static lv_obj_t *ui_label(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static lv_obj_t *ui_zh_label(lv_obj_t *parent, const char *text,
                             uint32_t color)
{
    return ui_label(parent, text, &weather_clock_font_16, color);
}

static lv_obj_t *ui_card(lv_obj_t *parent, int32_t x, int32_t y,
                         int32_t width, int32_t height, uint32_t color)
{
    lv_obj_t *card = lv_obj_create(parent);

    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, width, height);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    return card;
}

static lv_obj_t *ui_button(lv_obj_t *parent, int32_t x, int32_t y,
                           int32_t width, int32_t height, const char *text,
                           uint32_t color, lv_event_cb_t callback,
                           void *user_data)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_t *label;

    lv_obj_set_pos(button, x, y);
    lv_obj_set_size(button, width, height);
    lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);

    label = ui_zh_label(button, text, UI_TEXT);
    lv_obj_center(label);
    return button;
}

static uint8_t ui_is_leap_year(uint16_t year)
{
    return (uint8_t)(((year % 4U) == 0U && (year % 100U) != 0U) ||
                     (year % 400U) == 0U);
}

static uint8_t ui_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[] =
    {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if (month == 2U && ui_is_leap_year(year) != 0U)
        return 29U;
    return days[month - 1U];
}

static void ui_clock_add_second(void)
{
    s_clock.second++;
    if (s_clock.second < 60U)
        return;
    s_clock.second = 0U;
    s_clock.minute++;
    if (s_clock.minute < 60U)
        return;
    s_clock.minute = 0U;
    s_clock.hour++;
    if (s_clock.hour < 24U)
        return;
    s_clock.hour = 0U;
    s_clock.day++;
    if (s_clock.day <= ui_days_in_month(s_clock.year, s_clock.month))
        return;
    s_clock.day = 1U;
    s_clock.month++;
    if (s_clock.month <= 12U)
        return;
    s_clock.month = 1U;
    s_clock.year++;
}

static const char *ui_weather_name(void)
{
    unsigned int code = 99U;

    if (sscanf(s_weather.code, "%u", &code) != 1)
        return "--";
    if (code <= 3U)
        return "晴";
    if (code <= 9U)
        return "多云";
    if (code <= 19U)
        return "雨";
    if (code <= 25U)
        return "雪";
    if (code <= 30U)
        return "有风";
    if (code <= 39U)
        return "雾";
    if (code <= 99U)
        return "雷暴";
    return "未知";
}

static const char *ui_city_name(void)
{
    if (strcmp(s_settings.city, "beijing") == 0)
        return "北京";
    if (strcmp(s_settings.city, "shanghai") == 0)
        return "上海";
    if (strcmp(s_settings.city, "guangzhou") == 0)
        return "广州";
    if (strcmp(s_settings.city, "shenzhen") == 0)
        return "深圳";
    if (strcmp(s_settings.city, "hangzhou") == 0)
        return "杭州";
    if (strcmp(s_settings.city, "chengdu") == 0)
        return "成都";
    return s_settings.city;
}

static void ui_update_city(void)
{
    const char *name = ui_city_name();

    if (s_home_city_label != 0)
        lv_label_set_text(s_home_city_label, name);
    if (s_weather_city_label != 0)
        lv_label_set_text(s_weather_city_label, name);
    if (s_settings_city_label != 0)
        lv_label_set_text(s_settings_city_label, name);
}

static void ui_update_clock(void)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%02u:%02u:%02u",
                   s_clock.hour, s_clock.minute, s_clock.second);
    if (s_clock_label != 0)
        lv_label_set_text(s_clock_label, text);

    (void)snprintf(text, sizeof(text), "%04u / %02u / %02u",
                   s_clock.year, s_clock.month, s_clock.day);
    if (s_date_label != 0)
        lv_label_set_text(s_date_label, text);
}

static void ui_update_weather(void)
{
    char text[48];
    const char *name = s_weather.text[0] != '\0' ? s_weather.text : ui_weather_name();
    const char *temperature = s_weather.temperature[0] != '\0' ?
                               s_weather.temperature : "--";

    (void)snprintf(text, sizeof(text), "%s  %s°C", name, temperature);
    if (s_home_weather_label != 0)
        lv_label_set_text(s_home_weather_label, text);
    if (s_weather_summary_label != 0)
        lv_label_set_text(s_weather_summary_label, name);
    if (s_home_temperature_label != 0)
    {
        (void)snprintf(text, sizeof(text), "%s°C", temperature);
        lv_label_set_text(s_home_temperature_label, text);
    }
    if (s_weather_temperature_label != 0)
    {
        (void)snprintf(text, sizeof(text), "%s°C", temperature);
        lv_label_set_text(s_weather_temperature_label, text);
    }
    if (s_weather_code_label != 0)
    {
        (void)snprintf(text, sizeof(text), "天气代码：%s",
                       s_weather.code[0] != '\0' ? s_weather.code : "--");
        lv_label_set_text(s_weather_code_label, text);
    }
}

static void ui_update_network_status(void)
{
    const char *status = s_weather_stale != 0U ? "城市已修改，请点击同步" :
                         (s_wifi_online != 0U ? "网络已连接" : "离线模式");

    if (s_home_status_label != 0)
        lv_label_set_text(s_home_status_label, status);
    if (s_weather_status_label != 0)
        lv_label_set_text(s_weather_status_label, status);
    if (s_settings_status_label != 0)
        lv_label_set_text(s_settings_status_label, status);
}

static void ui_set_page(ui_page_t page)
{
    if (page >= UI_PAGE_COUNT)
        return;
    s_requested_page = page;
}

static void ui_nav_event(lv_event_t *event)
{
    ui_set_page((ui_page_t)(uintptr_t)lv_event_get_user_data(event));
}

static void ui_show_message(const char *title, const char *message)
{
    lv_obj_t *message_box = lv_msgbox_create(0);

    lv_obj_set_style_text_font(message_box, &weather_clock_font_16, 0);
    lv_msgbox_add_title(message_box, title);
    lv_msgbox_add_text(message_box, message);
    lv_msgbox_add_close_button(message_box);
}

static void ui_close_city_popup(void)
{
    if (s_city_popup != 0)
        lv_obj_delete_async(s_city_popup);
    s_city_popup = 0;
    s_city_textarea = 0;
}

static void ui_city_input_event(lv_event_t *event)
{
    const char *text;
    size_t length;

    if (lv_event_get_code(event) == LV_EVENT_CANCEL)
    {
        ui_close_city_popup();
        return;
    }
    if (lv_event_get_code(event) != LV_EVENT_READY || s_city_textarea == 0)
        return;

    text = lv_textarea_get_text(s_city_textarea);
    length = strlen(text);
    if (length > WEATHER_CITY_MAX_LEN)
        length = WEATHER_CITY_MAX_LEN;
    if (length == 0U)
        return;
    (void)memcpy(s_settings.city, text, length);
    s_settings.city[length] = '\0';
    (void)weather_clock_settings_save(&s_settings);
    ui_close_city_popup();
    ui_update_city();
    (void)memset(&s_weather, 0, sizeof(s_weather));
    s_weather_stale = 1U;
    ui_update_weather();
    ui_update_network_status();
}

static void ui_city_button_event(lv_event_t *event)
{
    lv_obj_t *keyboard;
    lv_obj_t *title;

    (void)event;
    if (s_city_popup != 0)
        return;
    s_city_popup = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_city_popup, 448, 300);
    lv_obj_center(s_city_popup);
    lv_obj_set_style_bg_color(s_city_popup, lv_color_hex(UI_PANEL), 0);
    lv_obj_set_style_bg_opa(s_city_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_city_popup, 12, 0);
    lv_obj_set_style_pad_all(s_city_popup, 16, 0);
    title = ui_zh_label(s_city_popup, "设置城市拼音", UI_TEXT);
    lv_obj_set_pos(title, 16, 14);
    s_city_textarea = lv_textarea_create(s_city_popup);
    lv_obj_set_size(s_city_textarea, 416, 54);
    lv_obj_set_pos(s_city_textarea, 16, 58);
    lv_textarea_set_one_line(s_city_textarea, true);
    lv_textarea_set_max_length(s_city_textarea, WEATHER_CITY_MAX_LEN);
    lv_textarea_set_accepted_chars(s_city_textarea,
                                   "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-");
    lv_textarea_set_text(s_city_textarea, s_settings.city);
    lv_obj_add_event_cb(s_city_textarea, ui_city_input_event, LV_EVENT_READY, 0);
    lv_obj_add_event_cb(s_city_textarea, ui_city_input_event, LV_EVENT_CANCEL, 0);
    keyboard = lv_keyboard_create(s_city_popup);
    lv_obj_set_width(keyboard, 416);
    lv_obj_set_height(keyboard, 150);
    lv_obj_set_pos(keyboard, 16, 125);
    lv_keyboard_set_textarea(keyboard, s_city_textarea);
}

static void ui_ota_event(lv_event_t *event)
{
    (void)event;
    ui_show_message("OTA 更新", "此功能暂未实现");
}

static void ui_touch_test_event(lv_event_t *event)
{
    (void)event;
    ui_show_message("触摸测试", "触摸输入正常，请点右上角关闭");
}

static void ui_brightness_event(lv_event_t *event)
{
    lv_obj_t *slider = lv_event_get_target(event);
    int32_t value = lv_slider_get_value(slider);
    char text[24];

    lcd_set_backlight((uint8_t)value);
    s_settings.brightness = (uint8_t)value;
    (void)snprintf(text, sizeof(text), "亮度  %ld%%", (long)value);
    if (s_settings_brightness_label != 0)
        lv_label_set_text(s_settings_brightness_label, text);
}

static void ui_brightness_release_event(lv_event_t *event)
{
    (void)event;
    (void)weather_clock_settings_save(&s_settings);
}

static void ui_auto_refresh_event(lv_event_t *event)
{
    lv_obj_t *switch_obj = lv_event_get_target(event);

    s_auto_refresh = lv_obj_has_state(switch_obj, LV_STATE_CHECKED) ? 1U : 0U;
}

static void ui_sync_data(void)
{
    esp8266_time_t network_time;
    esp8266_weather_t network_weather;

    if (s_weather_status_label != 0)
        lv_label_set_text(s_weather_status_label, "正在连接 Wi-Fi...");
    if (s_home_status_label != 0)
        lv_label_set_text(s_home_status_label, "正在连接 Wi-Fi...");

    esp8266_init(115200U);
    if (esp8266_connect_wifi(ESP8266_WIFI_SSID,
                             ESP8266_WIFI_PASSWORD) != 0U)
    {
        s_wifi_online = 0U;
        ui_update_network_status();
        return;
    }

    s_wifi_online = 1U;
    if (esp8266_sntp_get_time(8, ESP8266_SNTP_SERVER, &network_time) == 0U)
    {
        s_clock.year = network_time.year;
        s_clock.month = network_time.month;
        s_clock.day = network_time.day;
        s_clock.hour = network_time.hour;
        s_clock.minute = network_time.minute;
        s_clock.second = network_time.second;
        s_last_clock_ms = timer_millis();
        ui_update_clock();
    }

    if (esp8266_weather_get(s_settings.city, &network_weather,
                            10000UL) == 0U)
    {
        s_weather = network_weather;
        s_weather_stale = 0U;
        ui_update_weather();
    }
    ui_update_network_status();
}

static void ui_refresh_event(lv_event_t *event)
{
    (void)event;
    ui_sync_data();
}

static void ui_network_timer(lv_timer_t *timer)
{
    (void)timer;
    if (s_auto_refresh != 0U && s_wifi_online != 0U)
        ui_sync_data();
}

static void ui_create_home(lv_obj_t *parent)
{
    lv_obj_t *card;
    lv_obj_t *label;

    s_pages[UI_PAGE_HOME] = lv_obj_create(parent);
    lv_obj_set_size(s_pages[UI_PAGE_HOME], LCD_WIDTH, UI_NAV_Y);
    lv_obj_clear_flag(s_pages[UI_PAGE_HOME], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_pages[UI_PAGE_HOME], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[UI_PAGE_HOME], 0, 0);
    lv_obj_set_style_pad_all(s_pages[UI_PAGE_HOME], 0, 0);

    label = ui_zh_label(s_pages[UI_PAGE_HOME], "天气时钟", UI_TEXT);
    lv_obj_set_pos(label, UI_MARGIN, 18);
    s_home_city_label = ui_zh_label(s_pages[UI_PAGE_HOME], ui_city_name(), UI_MUTED);
    lv_obj_set_width(s_home_city_label, 180);
    lv_label_set_long_mode(s_home_city_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(s_home_city_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(s_home_city_label, LCD_WIDTH - 196, 20);

    s_clock_label = ui_label(s_pages[UI_PAGE_HOME], "22:06:38",
                             &lv_font_montserrat_48, UI_TEXT);
    lv_obj_set_width(s_clock_label, LCD_WIDTH);
    lv_obj_set_style_text_align(s_clock_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_clock_label, 0, 78);

    s_date_label = ui_label(s_pages[UI_PAGE_HOME], "2026 / 02 / 02",
                            &lv_font_montserrat_20, UI_MUTED);
    lv_obj_set_width(s_date_label, LCD_WIDTH);
    lv_obj_set_style_text_align(s_date_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_date_label, 0, 145);

    card = ui_card(s_pages[UI_PAGE_HOME], UI_MARGIN, 205,
                   UI_CONTENT_WIDTH, 190, UI_PANEL);
    label = ui_zh_label(card, "当前天气", UI_MUTED);
    lv_obj_set_pos(label, 22, 18);
    s_home_temperature_label = ui_label(card, "27 C",
                                        &lv_font_montserrat_32, UI_TEXT);
    lv_obj_set_pos(s_home_temperature_label, 22, 54);
    s_home_weather_label = ui_zh_label(card, "晴  27°C", UI_ACCENT);
    lv_obj_set_pos(s_home_weather_label, 22, 112);
    label = ui_zh_label(card, "室外温度", UI_MUTED);
    lv_obj_set_pos(label, 245, 58);
    label = ui_zh_label(card, "湿度  --%", UI_TEXT);
    lv_obj_set_pos(label, 245, 98);
    label = ui_zh_label(card, "风速  --", UI_TEXT);
    lv_obj_set_pos(label, 245, 130);

    card = ui_card(s_pages[UI_PAGE_HOME], UI_MARGIN, 415,
                   UI_CONTENT_WIDTH, 188, UI_PANEL_ALT);
    s_home_status_label = ui_zh_label(card, "离线模式", UI_TEXT);
    lv_obj_set_pos(s_home_status_label, 20, 24);
    label = ui_zh_label(card, "点击同步获取最新天气与时间", UI_MUTED);
    lv_obj_set_pos(label, 20, 72);
    label = ui_zh_label(card, "联网后每分钟自动刷新", UI_MUTED);
    lv_obj_set_pos(label, 20, 118);

    ui_button(s_pages[UI_PAGE_HOME], UI_MARGIN, 630, 212, 64,
              "立即同步", UI_ACCENT_BLUE, ui_refresh_event, 0);
    ui_button(s_pages[UI_PAGE_HOME], UI_MARGIN + 220, 630, 228, 64,
              "天气详情", UI_PANEL_ALT, ui_nav_event,
              (void *)(uintptr_t)UI_PAGE_WEATHER);
}

static void ui_create_weather(lv_obj_t *parent)
{
    lv_obj_t *card;
    lv_obj_t *label;

    s_pages[UI_PAGE_WEATHER] = lv_obj_create(parent);
    lv_obj_set_size(s_pages[UI_PAGE_WEATHER], LCD_WIDTH, UI_NAV_Y);
    lv_obj_clear_flag(s_pages[UI_PAGE_WEATHER], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_pages[UI_PAGE_WEATHER], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[UI_PAGE_WEATHER], 0, 0);
    lv_obj_set_style_pad_all(s_pages[UI_PAGE_WEATHER], 0, 0);

    label = ui_zh_label(s_pages[UI_PAGE_WEATHER], "天气详情", UI_TEXT);
    lv_obj_set_pos(label, UI_MARGIN, 18);
    label = ui_zh_label(s_pages[UI_PAGE_WEATHER], "心知天气实时数据", UI_MUTED);
    lv_obj_set_pos(label, UI_MARGIN, 51);

    card = ui_card(s_pages[UI_PAGE_WEATHER], UI_MARGIN, 92,
                   UI_CONTENT_WIDTH, 208, UI_PANEL);
    s_weather_city_label = ui_zh_label(card, ui_city_name(), UI_MUTED);
    lv_obj_set_width(s_weather_city_label, 380);
    lv_label_set_long_mode(s_weather_city_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_weather_city_label, 22, 20);
    s_weather_temperature_label = ui_label(card, "27 C",
                                           &lv_font_montserrat_48, UI_TEXT);
    lv_obj_set_pos(s_weather_temperature_label, 22, 52);
    s_weather_summary_label = ui_zh_label(card, "晴", UI_ACCENT);
    lv_obj_set_pos(s_weather_summary_label, 250, 73);
    label = ui_zh_label(card, "体感温度  --°C", UI_MUTED);
    lv_obj_set_pos(label, 250, 112);
    s_weather_code_label = ui_zh_label(card, "天气代码：0", UI_MUTED);
    lv_obj_set_pos(s_weather_code_label, 22, 166);

    card = ui_card(s_pages[UI_PAGE_WEATHER], UI_MARGIN, 325,
                   UI_CONTENT_WIDTH, 180, UI_PANEL_ALT);
    label = ui_zh_label(card, "今日", UI_ACCENT);
    lv_obj_set_pos(label, 20, 20);
    label = ui_zh_label(card, "最高  --°C", UI_TEXT);
    lv_obj_set_pos(label, 20, 62);
    label = ui_zh_label(card, "最低  --°C", UI_TEXT);
    lv_obj_set_pos(label, 20, 102);
    label = ui_zh_label(card, "湿度  --%", UI_TEXT);
    lv_obj_set_pos(label, 245, 62);
    label = ui_zh_label(card, "风速  --", UI_TEXT);
    lv_obj_set_pos(label, 245, 102);

    s_weather_status_label = ui_zh_label(s_pages[UI_PAGE_WEATHER], "离线模式", UI_MUTED);
    lv_obj_set_pos(s_weather_status_label, UI_MARGIN, 525);
    ui_button(s_pages[UI_PAGE_WEATHER], UI_MARGIN, 630, 212, 64,
              "刷新天气", UI_ACCENT_BLUE, ui_refresh_event, 0);
    ui_button(s_pages[UI_PAGE_WEATHER], UI_MARGIN + 220, 630, 228, 64,
              "返回首页", UI_PANEL_ALT, ui_nav_event,
              (void *)(uintptr_t)UI_PAGE_HOME);
}

static void ui_create_settings(lv_obj_t *parent)
{
    lv_obj_t *card;
    lv_obj_t *label;
    lv_obj_t *slider;
    lv_obj_t *switch_obj;

    s_pages[UI_PAGE_SETTINGS] = lv_obj_create(parent);
    lv_obj_set_size(s_pages[UI_PAGE_SETTINGS], LCD_WIDTH, UI_NAV_Y);
    lv_obj_clear_flag(s_pages[UI_PAGE_SETTINGS], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_pages[UI_PAGE_SETTINGS], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_pages[UI_PAGE_SETTINGS], 0, 0);
    lv_obj_set_style_pad_all(s_pages[UI_PAGE_SETTINGS], 0, 0);

    label = ui_zh_label(s_pages[UI_PAGE_SETTINGS], "设置", UI_TEXT);
    lv_obj_set_pos(label, UI_MARGIN, 18);
    s_settings_status_label = ui_zh_label(s_pages[UI_PAGE_SETTINGS], "离线模式", UI_MUTED);
    lv_obj_set_pos(s_settings_status_label, UI_MARGIN, 53);

    card = ui_card(s_pages[UI_PAGE_SETTINGS], UI_MARGIN, 92,
                   UI_CONTENT_WIDTH, 136, UI_PANEL);
    label = ui_zh_label(card, "城市", UI_MUTED);
    lv_obj_set_pos(label, 20, 18);
    s_settings_city_label = ui_zh_label(card, ui_city_name(), UI_TEXT);
    lv_obj_set_width(s_settings_city_label, 210);
    lv_label_set_long_mode(s_settings_city_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(s_settings_city_label, 20, 52);
    ui_button(card, 250, 42, 150, 48, "修改城市", UI_ACCENT_BLUE,
              ui_city_button_event, 0);
    label = ui_zh_label(card, "天气服务：心知天气", UI_MUTED);
    lv_obj_set_pos(label, 20, 94);

    card = ui_card(s_pages[UI_PAGE_SETTINGS], UI_MARGIN, 245,
                   UI_CONTENT_WIDTH, 118, UI_PANEL_ALT);
    s_settings_brightness_label = ui_zh_label(card, "亮度  50%", UI_TEXT);
    lv_obj_set_pos(s_settings_brightness_label, 20, 18);
    {
        char text[24];

        (void)snprintf(text, sizeof(text), "亮度  %u%%", s_settings.brightness);
        lv_label_set_text(s_settings_brightness_label, text);
    }
    slider = lv_slider_create(card);
    lv_obj_set_width(slider, 390);
    lv_obj_set_pos(slider, 20, 63);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, s_settings.brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, ui_brightness_event, LV_EVENT_VALUE_CHANGED, 0);
    lv_obj_add_event_cb(slider, ui_brightness_release_event, LV_EVENT_RELEASED, 0);

    card = ui_card(s_pages[UI_PAGE_SETTINGS], UI_MARGIN, 380,
                   UI_CONTENT_WIDTH, 76, UI_PANEL);
    label = ui_zh_label(card, "自动刷新天气", UI_TEXT);
    lv_obj_set_pos(label, 20, 24);
    switch_obj = lv_switch_create(card);
    lv_obj_set_pos(switch_obj, 365, 17);
    if (s_auto_refresh != 0U)
        lv_obj_add_state(switch_obj, LV_STATE_CHECKED);
    lv_obj_add_event_cb(switch_obj, ui_auto_refresh_event,
                        LV_EVENT_VALUE_CHANGED, 0);

    ui_button(s_pages[UI_PAGE_SETTINGS], UI_MARGIN, 480, 212, 58,
              "触摸测试", UI_PANEL_ALT, ui_touch_test_event, 0);
    ui_button(s_pages[UI_PAGE_SETTINGS], UI_MARGIN + 220, 480, 228, 58,
              "OTA 更新", UI_WARNING, ui_ota_event, 0);
    ui_button(s_pages[UI_PAGE_SETTINGS], UI_MARGIN, 630, UI_CONTENT_WIDTH,
              64, "返回首页", UI_ACCENT, ui_nav_event,
              (void *)(uintptr_t)UI_PAGE_HOME);
}

static void ui_create_navigation(lv_obj_t *parent)
{
    lv_obj_t *bar = ui_card(parent, 0, UI_NAV_Y, LCD_WIDTH, 88, UI_PANEL);
    lv_obj_t *button;
    lv_obj_t *label;
    uint8_t index;
    const char *names[UI_PAGE_COUNT] = {"首页", "天气", "设置"};

    s_navigation = bar;

    for (index = 0U; index < UI_PAGE_COUNT; index++)
    {
        button = lv_button_create(bar);
        lv_obj_set_size(button, 140, 64);
        lv_obj_set_pos(button, 10 + (int32_t)index * 155, 12);
        lv_obj_set_style_bg_color(button, lv_color_hex(s_page_colors[index]), 0);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(button, 12, 0);
        lv_obj_set_style_border_width(button, 0, 0);
        lv_obj_add_event_cb(button, ui_nav_event, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)index);
        label = ui_zh_label(button, names[index], UI_TEXT);
        lv_obj_center(label);
    }
}

void weather_clock_ui_init(void)
{
    lv_obj_t *screen = lv_screen_active();

    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    weather_clock_settings_load(&s_settings);
    ui_create_navigation(screen);
    ui_set_page(UI_PAGE_HOME);
    s_last_clock_ms = timer_millis();
    weather_clock_ui_process();
    (void)lv_timer_create(ui_network_timer, 60000U, 0);
}

void weather_clock_ui_process(void)
{
    uint32_t now = timer_millis();
    lv_obj_t *screen;

    if (s_requested_page < UI_PAGE_COUNT &&
        s_requested_page != s_current_page)
    {
        if (s_current_page < UI_PAGE_COUNT)
        {
            lv_obj_delete(s_pages[s_current_page]);
            s_pages[s_current_page] = 0;
        }

        s_clock_label = 0;
        s_date_label = 0;
        s_home_weather_label = 0;
        s_home_temperature_label = 0;
        s_home_status_label = 0;
        s_weather_temperature_label = 0;
        s_weather_summary_label = 0;
        s_weather_code_label = 0;
        s_weather_status_label = 0;
        s_settings_brightness_label = 0;
        s_settings_status_label = 0;
        s_settings_city_label = 0;
        s_home_city_label = 0;
        s_weather_city_label = 0;

        screen = lv_screen_active();
        if (s_requested_page == UI_PAGE_HOME)
            ui_create_home(screen);
        else if (s_requested_page == UI_PAGE_WEATHER)
            ui_create_weather(screen);
        else
            ui_create_settings(screen);
        s_current_page = s_requested_page;
        lv_obj_move_to_index(s_navigation, -1);
        ui_update_clock();
        ui_update_city();
        ui_update_weather();
        ui_update_network_status();
    }

    while ((uint32_t)(now - s_last_clock_ms) >= 1000U)
    {
        s_last_clock_ms += 1000U;
        ui_clock_add_second();
        ui_update_clock();
    }
}
