#include "board.h"
#include "delay.h"
#include "lcd.h"
#include "touch.h"
#include "usart1.h"

#include <stdint.h>

static void touch_draw_dialog(void)
{
    lcd_clear(LCD_WHITE);
    lcd_set_text_color(LCD_BLUE);
    lcd_set_back_color(LCD_WHITE);
    lcd_set_window_clear(1U);
    lcd_show_string(LCD_WIDTH - 32U, 0U, 32U, 24U, "RST", 16U);
    lcd_set_window_clear(0U);
}

static void touch_draw_point(uint16_t x, uint16_t y)
{
    lcd_draw_point(x, y);
    if (x + 1U < LCD_WIDTH)
        lcd_draw_point((uint16_t)(x + 1U), y);
    if (y + 1U < LCD_HEIGHT)
        lcd_draw_point(x, (uint16_t)(y + 1U));
    if (x + 1U < LCD_WIDTH && y + 1U < LCD_HEIGHT)
        lcd_draw_point((uint16_t)(x + 1U), (uint16_t)(y + 1U));
}

static void touch_draw_hline(uint16_t x, uint16_t y, uint16_t length)
{
    uint16_t index;

    for (index = 0U; index < length; index++)
    {
        if (x + index >= LCD_WIDTH || y >= LCD_HEIGHT)
            break;
        touch_draw_point((uint16_t)(x + index), y);
    }
}

/* Same filled-circle brush used by experiment 27's lcd_draw_bline(). */
static void touch_fill_circle(uint16_t x0, uint16_t y0, uint16_t radius)
{
    uint32_t i;
    uint32_t max_i = ((uint32_t)radius * 707U) / 1000U + 1U;
    uint32_t max_square = (uint32_t)radius * radius + radius / 2U;
    uint16_t x = radius;

    touch_draw_hline(x0 >= radius ? (uint16_t)(x0 - radius) : 0U,
                     y0, (uint16_t)(2U * radius));
    for (i = 1U; i <= max_i; i++)
    {
        if (i * i + (uint32_t)x * x > max_square)
        {
            if (x > max_i)
            {
                if (x0 >= i - 1U)
                    touch_draw_hline((uint16_t)(x0 - i + 1U),
                                     (uint16_t)(y0 + x),
                                     (uint16_t)(2U * (i - 1U)));
                if (y0 >= x)
                    touch_draw_hline((uint16_t)(x0 >= i - 1U ?
                                                 x0 - i + 1U : 0U),
                                     (uint16_t)(y0 - x),
                                     (uint16_t)(2U * (i - 1U)));
            }
            x--;
        }
        if (y0 + i < LCD_HEIGHT)
            touch_draw_hline(x0 >= x ? (uint16_t)(x0 - x) : 0U,
                             (uint16_t)(y0 + i), (uint16_t)(2U * x));
        if (y0 >= i)
            touch_draw_hline(x0 >= x ? (uint16_t)(x0 - x) : 0U,
                             (uint16_t)(y0 - i), (uint16_t)(2U * x));
    }
}

static void touch_draw_bline(uint16_t x1, uint16_t y1,
                             uint16_t x2, uint16_t y2, uint8_t size)
{
    uint16_t step;
    int32_t x_error = 0;
    int32_t y_error = 0;
    int32_t delta_x = (int32_t)x2 - x1;
    int32_t delta_y = (int32_t)y2 - y1;
    int32_t distance;
    int32_t increment_x;
    int32_t increment_y;
    uint16_t row = x1;
    uint16_t column = y1;

    if (x1 < size || x2 < size || y1 < size || y2 < size)
        return;
    increment_x = delta_x > 0 ? 1 : (delta_x == 0 ? 0 : -1);
    increment_y = delta_y > 0 ? 1 : (delta_y == 0 ? 0 : -1);
    if (delta_x < 0)
        delta_x = -delta_x;
    if (delta_y < 0)
        delta_y = -delta_y;
    distance = delta_x > delta_y ? delta_x : delta_y;

    for (step = 0U; step <= (uint16_t)(distance + 1); step++)
    {
        touch_fill_circle(row, column, size);
        x_error += delta_x;
        y_error += delta_y;
        if (x_error > distance)
        {
            x_error -= distance;
            row = (uint16_t)((int32_t)row + increment_x);
        }
        if (y_error > distance)
        {
            y_error -= distance;
            column = (uint16_t)((int32_t)column + increment_y);
        }
    }
}

static void touch_draw_test(void)
{
    uint16_t last_x = 0xFFFFU;
    uint16_t last_y = 0xFFFFU;
    touch_point_t point;

    for (;;)
    {
        (void)touch_scan(&point);
        if (point.pressed != 0U)
        {
            if (point.x >= LCD_WIDTH - 24U && point.y < 16U)
            {
                touch_draw_dialog();
                last_x = 0xFFFFU;
                last_y = 0xFFFFU;
            }
            else if (point.x < LCD_WIDTH && point.y < LCD_HEIGHT)
            {
                if (last_x == 0xFFFFU)
                {
                    last_x = point.x;
                    last_y = point.y;
                }
                touch_draw_bline(last_x, last_y, point.x, point.y, 2U);
                last_x = point.x;
                last_y = point.y;
            }
        }
        else
        {
            last_x = 0xFFFFU;
            last_y = 0xFFFFU;
        }
        delay_ms(5U);
    }
}

int main(void)
{
    board_lowlevel_init();
    delay_init();
    usart1_init(115200U);
    lcd_init();
    lcd_set_text_color(LCD_BLUE);
    lcd_set_back_color(LCD_WHITE);
    (void)touch_init();
    touch_draw_dialog();
    usart1_send_string("Experiment 27 touch draw ready\r\n");
    touch_draw_test();
    return 0;
}
