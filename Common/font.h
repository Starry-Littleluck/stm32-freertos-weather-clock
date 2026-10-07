#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

#define WEATHER_ICON_WIDTH 16U
#define WEATHER_ICON_HEIGHT 16U
#define FONT_CODE_GBK_FLAG 0x01000000UL

typedef enum
{
    WEATHER_ICON_SUNNY = 0,
    WEATHER_ICON_CLOUDY,
    WEATHER_ICON_RAIN,
    WEATHER_ICON_SNOW,
    WEATHER_ICON_THUNDER,
    WEATHER_ICON_FOG,
    WEATHER_ICON_COUNT
} weather_icon_id_t;

extern const uint16_t weather_icons[WEATHER_ICON_COUNT][WEATHER_ICON_HEIGHT];
extern const uint8_t asc2_1206[95][12];
extern const uint8_t asc2_1608[95][16];
extern const uint8_t asc2_2412[95][36];
extern const uint8_t asc2_3216[95][64];

const uint8_t *font_get_ascii_glyph(uint8_t data, uint8_t size,
                                    uint8_t *width, uint8_t *bytes);
uint8_t font_get_char_width(uint32_t code, uint8_t size);
uint8_t font_decode_text(const uint8_t *text, uint32_t *code);

/* The target project has no external Chinese font storage yet. */
uint8_t font_read_glyph(uint32_t code, uint8_t size, uint8_t *glyph);

#endif
