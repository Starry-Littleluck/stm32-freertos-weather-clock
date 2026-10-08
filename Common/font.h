#ifndef __FONT_H
#define __FONT_H

#include <stdint.h>

#define WEATHER_ICON_WIDTH 16U
#define WEATHER_ICON_HEIGHT 16U

#define FONT_UNIGBK_SIZE 174344UL
#define FONT_GBK12_SIZE 574560UL
#define FONT_GBK16_SIZE 766080UL
#define FONT_GBK24_SIZE 1723680UL
#define FONT_FLASH_BASE_ADDRESS 0x400000UL
#define FONT_FLASH_SECTOR_SIZE 4096UL
#define FONT_FLASH_UNIGBK_ADDRESS FONT_FLASH_BASE_ADDRESS
#define FONT_FLASH_GBK12_ADDRESS \
    (FONT_FLASH_BASE_ADDRESS + ((FONT_UNIGBK_SIZE + \
                                 FONT_FLASH_SECTOR_SIZE - 1UL) & \
                                ~(FONT_FLASH_SECTOR_SIZE - 1UL)))
#define FONT_FLASH_GBK16_ADDRESS \
    (FONT_FLASH_GBK12_ADDRESS + ((FONT_GBK12_SIZE + \
                                  FONT_FLASH_SECTOR_SIZE - 1UL) & \
                                 ~(FONT_FLASH_SECTOR_SIZE - 1UL)))
#define FONT_FLASH_GBK24_ADDRESS \
    (FONT_FLASH_GBK16_ADDRESS + ((FONT_GBK16_SIZE + \
                                  FONT_FLASH_SECTOR_SIZE - 1UL) & \
                                 ~(FONT_FLASH_SECTOR_SIZE - 1UL)))

#define FONT_CODE_GBK_FLAG 0x01000000UL
#define FONT_CODE_IS_GBK(code) (((code) & FONT_CODE_GBK_FLAG) != 0UL)
#define FONT_CODE_GBK_VALUE(code) ((uint16_t)((code) & 0xFFFFUL))

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

uint8_t font_flash_init(void); /* 初始化 W25Q128，0 表示成功 */
uint8_t font_flash_read(uint32_t address, uint8_t *data, uint32_t length);
uint8_t font_read_glyph(uint32_t code, uint8_t size, uint8_t *glyph);

#endif
