#ifndef __LCD_H
#define __LCD_H

#include "stm32f10x.h"

/* 实测 LCD 控制器为 NT5510，默认方向为竖屏 480x800。 */
#define LCD_WIDTH  480U
#define LCD_HEIGHT 800U

/* FSMC Bank1 NOR/SRAM4, with A10 used as the command/data select line. */
#define LCD_BASE ((uint32_t)(0x6C000000UL | 0x000007FEUL))

/* Set to 1 when the panel reset line is wired to PB1. */
#define LCD_USE_RESET 0
#define LCD_RESET_PORT GPIOB
#define LCD_RESET_PIN GPIO_Pin_1

#define LCD_WHITE   0xFFFFU
#define LCD_BLACK   0x0000U
#define LCD_BLUE    0x001FU
#define LCD_RED     0xF800U
#define LCD_GREEN   0x07E0U
#define LCD_CYAN    0x7FFFU
#define LCD_YELLOW  0xFFE0U
#define LCD_GRAY    0x8430U

void lcd_init(void);
void lcd_set_backlight(uint8_t percent);
void lcd_set_text_color(uint16_t color);
void lcd_set_back_color(uint16_t color);
void lcd_set_window_clear(uint8_t enable);
void lcd_display_on(void);
void lcd_display_off(void);
void lcd_clear(uint16_t color);
void lcd_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
              uint16_t color);
void lcd_draw_point(uint16_t x, uint16_t y);
void lcd_set_cursor(uint16_t x, uint16_t y);
void lcd_set_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height);
void lcd_write_pixels(const uint16_t *pixels, uint32_t count);
void lcd_show_char(uint16_t x, uint16_t y, uint8_t data, uint8_t size);
void lcd_show_num(uint16_t x, uint16_t y, uint32_t number, uint8_t length,
                  uint8_t size);
void lcd_show_signed_num(uint16_t x, uint16_t y, int32_t number,
                         uint8_t length, uint8_t size);
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     const char *text, uint8_t size);

#endif
