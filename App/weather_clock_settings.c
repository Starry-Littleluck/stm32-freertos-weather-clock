#include "weather_clock_settings.h"

#include "at24c02.h"
#include <string.h>

#define SETTINGS_ADDRESS 192U
#define SETTINGS_MAGIC_0 0x57U
#define SETTINGS_MAGIC_1 0x43U
#define SETTINGS_RECORD_SIZE 36U

static uint8_t settings_checksum(const uint8_t *data)
{
    uint8_t sum = 0U;
    uint8_t index;

    for (index = 0U; index < SETTINGS_RECORD_SIZE - 1U; index++)
        sum = (uint8_t)(sum + data[index]);
    return sum;
}

void weather_clock_settings_load(weather_clock_settings_t *settings)
{
    uint8_t record[SETTINGS_RECORD_SIZE];

    if (settings == 0)
        return;
    (void)strcpy(settings->city, "beijing");
    settings->brightness = 50U;

    if (at24c02_init() != 0U ||
        at24c02_read(SETTINGS_ADDRESS, record, sizeof(record)) != 0U)
        return;
    if (record[0] != SETTINGS_MAGIC_0 || record[1] != SETTINGS_MAGIC_1 ||
        record[SETTINGS_RECORD_SIZE - 1U] != settings_checksum(record) ||
        record[2] < 1U || record[2] > 100U || record[3] == 0U)
        return;

    record[34] = 0U;
    (void)memcpy(settings->city, &record[3], sizeof(settings->city));
    settings->city[WEATHER_CITY_MAX_LEN] = '\0';
    settings->brightness = record[2];
}

uint8_t weather_clock_settings_save(const weather_clock_settings_t *settings)
{
    uint8_t record[SETTINGS_RECORD_SIZE] = {0U};

    if (settings == 0)
        return 1U;
    record[0] = SETTINGS_MAGIC_0;
    record[1] = SETTINGS_MAGIC_1;
    record[2] = settings->brightness;
    (void)memcpy(&record[3], settings->city, sizeof(settings->city));
    record[SETTINGS_RECORD_SIZE - 1U] = settings_checksum(record);
    return at24c02_write(SETTINGS_ADDRESS, record, sizeof(record));
}
