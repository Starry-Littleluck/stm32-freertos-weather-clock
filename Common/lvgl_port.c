#include "lvgl_port.h"

#include "lcd.h"
#include "touch.h"

#define LVGL_PORT_BUF_LINES   10U

static uint16_t s_lvgl_draw_buf[LCD_WIDTH * LVGL_PORT_BUF_LINES];
static lv_display_t *s_lvgl_display;
static lv_point_t s_last_touch_point;

static void lvgl_flush_cb(lv_display_t *display, const lv_area_t *area,
                          uint8_t *pixel_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    uint32_t width;
    uint32_t height;

    if (x1 < 0)
        x1 = 0;
    if (y1 < 0)
        y1 = 0;
    if (x2 >= (int32_t)LCD_WIDTH)
        x2 = (int32_t)LCD_WIDTH - 1;
    if (y2 >= (int32_t)LCD_HEIGHT)
        y2 = (int32_t)LCD_HEIGHT - 1;

    if (x1 <= x2 && y1 <= y2)
    {
        width = (uint32_t)(x2 - x1 + 1);
        height = (uint32_t)(y2 - y1 + 1);
        lcd_set_window((uint16_t)x1, (uint16_t)y1,
                       (uint16_t)width, (uint16_t)height);
        lcd_write_pixels((const uint16_t *)pixel_map, width * height);
    }

    lv_display_flush_ready(display);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    touch_point_t point = {0U, 0U, 0U};

    (void)indev;
    (void)touch_scan(&point);
    data->state = point.pressed != 0U ? LV_INDEV_STATE_PRESSED
                                     : LV_INDEV_STATE_RELEASED;
    if (point.pressed != 0U && point.x < TOUCH_WIDTH &&
        point.y < TOUCH_HEIGHT)
    {
        s_last_touch_point.x = (lv_coord_t)point.x;
        s_last_touch_point.y = (lv_coord_t)point.y;
    }
    data->point = s_last_touch_point;
}

lv_display_t *lvgl_port_init(void)
{
    lv_indev_t *touch_indev;

    s_lvgl_display = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    if (s_lvgl_display == 0)
    {
        return 0;
    }

    lv_display_set_buffers(s_lvgl_display, s_lvgl_draw_buf, 0,
                           sizeof(s_lvgl_draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_lvgl_display, lvgl_flush_cb);
    lv_display_set_default(s_lvgl_display);

    touch_indev = lv_indev_create();
    if (touch_indev != 0)
    {
        lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
        lv_indev_set_display(touch_indev, s_lvgl_display);
    }
    return s_lvgl_display;
}
