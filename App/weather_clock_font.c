#include "weather_clock_font.h"

#include "font.h"

static bool weather_clock_font_get_dsc(const lv_font_t *font,
                                       lv_font_glyph_dsc_t *dsc,
                                       uint32_t letter,
                                       uint32_t next)
{
    uint8_t glyph[32];

    (void)font;
    (void)next;
    if (letter < 0x80U || font_read_glyph(letter, 16U, glyph) != 0U)
        return false;
    dsc->adv_w = 16U;
    dsc->box_w = 16U;
    dsc->box_h = 16U;
    dsc->ofs_x = 0;
    dsc->ofs_y = 0;
    dsc->stride = 16U;
    dsc->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc->is_placeholder = false;
    dsc->gid.index = letter;
    return true;
}

static const void *weather_clock_font_get_bitmap(lv_font_glyph_dsc_t *dsc,
                                                 lv_draw_buf_t *draw_buf)
{
    uint8_t glyph[32];
    uint8_t row;
    uint8_t column;
    uint8_t *bitmap;

    if (draw_buf == 0 || draw_buf->data == 0 ||
        draw_buf->header.stride < 16U ||
        draw_buf->data_size < (uint32_t)draw_buf->header.stride * 16U ||
        font_read_glyph(dsc->gid.index, 16U, glyph) != 0U)
        return 0;
    bitmap = draw_buf->data;
    for (row = 0U; row < 16U; row++)
    {
        for (column = 0U; column < 16U; column++)
        {
            bitmap[(uint32_t)row * draw_buf->header.stride + column] =
                (glyph[(uint16_t)column * 2U + row / 8U] &
                 (uint8_t)(0x80U >> (row % 8U))) != 0U ? 0xFFU : 0U;
        }
    }
    return draw_buf;
}

const lv_font_t weather_clock_font_16 =
{
    .get_glyph_dsc = weather_clock_font_get_dsc,
    .get_glyph_bitmap = weather_clock_font_get_bitmap,
    .release_glyph = 0,
    .line_height = 20,
    .base_line = 4,
    .subpx = LV_FONT_SUBPX_NONE,
    .kerning = LV_FONT_KERNING_NONE,
    .static_bitmap = 0,
    .underline_position = -2,
    .underline_thickness = 1,
    .dsc = 0,
    .fallback = &lv_font_montserrat_16,
    .user_data = 0
};
