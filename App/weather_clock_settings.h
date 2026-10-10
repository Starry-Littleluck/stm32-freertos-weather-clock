#ifndef WEATHER_CLOCK_SETTINGS_H
#define WEATHER_CLOCK_SETTINGS_H

#include <stdint.h>

#define WEATHER_CITY_MAX_LEN 31U

typedef struct
{
    char city[WEATHER_CITY_MAX_LEN + 1U];
    uint8_t brightness;
} weather_clock_settings_t;

void weather_clock_settings_load(weather_clock_settings_t *settings);
uint8_t weather_clock_settings_save(const weather_clock_settings_t *settings);

#endif
