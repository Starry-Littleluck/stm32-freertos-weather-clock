/**
 * @file font_download_demo.c
 * @brief 通过 USART1 下载字库到 W25Q128。
 *
 * 协议与 project 工程的 upload_fonts.ps1 对应：
 *   FONT                  4 字节同步头
 *   file_id               1 字节，0=UNIGBK，1=GBK12，2=GBK16，3=GBK24
 *   file_size             4 字节，小端
 *   file_crc32            4 字节，小端
 *   data                  连续文件数据，设备端只在文件末尾校验一次 CRC32
 */

#include "font_download_demo.h"

#include "font.h"
#include "lcd.h"
#include "usart1.h"
#include "w25q128.h"
#include "board.h"
#include "delay.h"
#include <stdint.h>

#define FONT_DOWNLOAD_BLOCK_SIZE 1024U
#define FONT_DOWNLOAD_MAGIC "FONT"
#define FONT_DOWNLOAD_MAGIC_SIZE 4U
#define FONT_DOWNLOAD_PROGRESS_X 20U
#define FONT_DOWNLOAD_PROGRESS_Y 160U
#define FONT_DOWNLOAD_PROGRESS_WIDTH 340U
#define FONT_DOWNLOAD_PROGRESS_HEIGHT 20U
#define FONT_DOWNLOAD_PERCENT_X 370U

typedef enum
{
    FONT_DOWNLOAD_IDLE = 0,
    FONT_DOWNLOAD_HEADER,
    FONT_DOWNLOAD_DATA
} font_download_state_t;

static font_download_state_t s_state;
static uint8_t s_magic_index;
static uint8_t s_header[9];
static uint8_t s_header_index;
static uint8_t s_file_id;
static uint32_t s_file_address;
static uint32_t s_file_size;
static uint32_t s_file_remaining;
static uint32_t s_file_crc_expected;
static uint32_t s_file_crc;
static uint16_t s_block_size;
static uint16_t s_block_index;
static uint8_t s_block[FONT_DOWNLOAD_BLOCK_SIZE];
static uint8_t s_flash_ready;
static uint8_t s_lcd_percent;

static const char *font_download_get_name(uint8_t file_id)
{
    switch (file_id)
    {
        case 0U: return "UNIGBK.BIN";
        case 1U: return "GBK12.FON";
        case 2U: return "GBK16.FON";
        case 3U: return "GBK24.FON";
        default: return "UNKNOWN";
    }
}

static uint32_t font_download_crc32_update(uint32_t crc,
                                           const uint8_t *data,
                                           uint16_t length)
{
    uint16_t index;
    uint8_t bit;

    for (index = 0U; index < length; index++)
    {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = (crc & 1UL) != 0UL ?
                (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
        }
    }
    return crc;
}

static void font_download_show(const char *text)
{
    lcd_show_string(20U, 200U, LCD_WIDTH - 40U, 24U, text, 16U);
}

static void font_download_show_progress(uint32_t completed,
                                        uint32_t total)
{
    uint8_t percent;
    uint16_t fill_width;

    if (total == 0UL)
    {
        return;
    }
    percent = (uint8_t)((completed * 100UL) / total);
    if (percent == s_lcd_percent)
    {
        return;
    }
    s_lcd_percent = percent;

    /* Draw the complete bar on each percentage change so it also clears
       the previous fill when a new erase/write phase starts at zero. */
    lcd_fill(FONT_DOWNLOAD_PROGRESS_X,
             FONT_DOWNLOAD_PROGRESS_Y,
             FONT_DOWNLOAD_PROGRESS_WIDTH,
             FONT_DOWNLOAD_PROGRESS_HEIGHT,
             LCD_GRAY);
    lcd_fill((uint16_t)(FONT_DOWNLOAD_PROGRESS_X + 2U),
             (uint16_t)(FONT_DOWNLOAD_PROGRESS_Y + 2U),
             (uint16_t)(FONT_DOWNLOAD_PROGRESS_WIDTH - 4U),
             (uint16_t)(FONT_DOWNLOAD_PROGRESS_HEIGHT - 4U),
             LCD_WHITE);
    fill_width = (uint16_t)(((uint32_t)(FONT_DOWNLOAD_PROGRESS_WIDTH - 4U) *
                             percent) / 100UL);
    if (fill_width != 0U)
    {
        lcd_fill((uint16_t)(FONT_DOWNLOAD_PROGRESS_X + 2U),
                 (uint16_t)(FONT_DOWNLOAD_PROGRESS_Y + 2U),
                 fill_width,
                 (uint16_t)(FONT_DOWNLOAD_PROGRESS_HEIGHT - 4U),
                 LCD_BLUE);
    }
    lcd_show_num(FONT_DOWNLOAD_PERCENT_X, FONT_DOWNLOAD_PROGRESS_Y,
                 percent, 3U, 16U);
    lcd_show_char((uint16_t)(FONT_DOWNLOAD_PERCENT_X + 24U),
                  FONT_DOWNLOAD_PROGRESS_Y, '%', 16U);
}

static void font_download_send(uint8_t value)
{
    usart1_send(&value, 1U);
}

static uint8_t font_download_read_byte(uint8_t *value)
{
    return usart1_read(value, 1U) == 1U ? 1U : 0U;
}

static void font_download_reset(void)
{
    s_state = FONT_DOWNLOAD_IDLE;
    s_magic_index = 0U;
    s_header_index = 0U;
    s_block_index = 0U;
    s_file_remaining = 0UL;
    s_lcd_percent = 0xFFU;
}

static uint32_t font_download_get_address(uint8_t file_id)
{
    switch (file_id)
    {
        case 0U: return FONT_FLASH_UNIGBK_ADDRESS;
        case 1U: return FONT_FLASH_GBK12_ADDRESS;
        case 2U: return FONT_FLASH_GBK16_ADDRESS;
        case 3U: return FONT_FLASH_GBK24_ADDRESS;
        default: return 0UL;
    }
}

static uint32_t font_download_get_size(uint8_t file_id)
{
    switch (file_id)
    {
        case 0U: return FONT_UNIGBK_SIZE;
        case 1U: return FONT_GBK12_SIZE;
        case 2U: return FONT_GBK16_SIZE;
        case 3U: return FONT_GBK24_SIZE;
        default: return 0UL;
    }
}

static uint8_t font_download_erase_range(uint32_t address, uint32_t length)
{
    uint32_t end_address;
    uint32_t first_sector;
    uint32_t total_sectors;
    uint32_t completed_sectors = 0UL;

    if (length == 0UL || address >= W25Q128_CAPACITY ||
        length > W25Q128_CAPACITY - address)
    {
        return 1U;
    }

    end_address = address + length;
    first_sector = address & ~(W25Q128_SECTOR_SIZE - 1UL);
    total_sectors = (end_address - first_sector +
                     W25Q128_SECTOR_SIZE - 1UL) / W25Q128_SECTOR_SIZE;
    address &= ~(W25Q128_SECTOR_SIZE - 1UL);
    font_download_show("ERASING FLASH...");
    font_download_show_progress(0UL, total_sectors);
    while (address < end_address)
    {
        if (w25q128_erase_sector(address) != 0U)
        {
            return 1U;
        }
        address += W25Q128_SECTOR_SIZE;
        completed_sectors++;
        font_download_show_progress(completed_sectors, total_sectors);
    }
    return 0U;
}

static uint8_t font_download_process_header(void)
{
    s_file_id = s_header[0];
    s_file_size = (uint32_t)s_header[1] |
                  ((uint32_t)s_header[2] << 8) |
                  ((uint32_t)s_header[3] << 16) |
                  ((uint32_t)s_header[4] << 24);
    s_file_crc_expected = (uint32_t)s_header[5] |
                          ((uint32_t)s_header[6] << 8) |
                          ((uint32_t)s_header[7] << 16) |
                          ((uint32_t)s_header[8] << 24);
    s_file_address = font_download_get_address(s_file_id);

    if (s_file_address == 0UL || s_file_size != font_download_get_size(s_file_id))
    {
        font_download_show("FONT HEADER ERROR");
        font_download_send('H');
        font_download_reset();
        return 1U;
    }

    lcd_show_string(65U, 120U, 250U, 24U,
                    font_download_get_name(s_file_id), 16U);
    if (font_download_erase_range(s_file_address, s_file_size) != 0U)
    {
        font_download_show("FONT ERASE FAILED");
        font_download_send('E');
        font_download_reset();
        return 1U;
    }

    s_file_remaining = s_file_size;
    s_file_crc = 0xFFFFFFFFUL;
    s_block_size = s_file_remaining > FONT_DOWNLOAD_BLOCK_SIZE ?
                   FONT_DOWNLOAD_BLOCK_SIZE : (uint16_t)s_file_remaining;
    s_block_index = 0U;
    s_state = FONT_DOWNLOAD_DATA;
    font_download_show("RECEIVING FONT...");
    font_download_show_progress(0UL, s_file_size);
    font_download_send('R');
    return 0U;
}

static void font_download_process_block(void)
{
    uint32_t address;
    uint8_t status;

    s_file_crc = font_download_crc32_update(s_file_crc, s_block, s_block_size);
    address = s_file_address + (s_file_size - s_file_remaining);
    status = w25q128_write(address, s_block, s_block_size);
    if (status != 0U)
    {
        font_download_show("FONT WRITE FAILED");
        font_download_send('W');
        font_download_reset();
        return;
    }

    s_file_remaining -= s_block_size;
    font_download_show_progress(s_file_size - s_file_remaining, s_file_size);
    if (s_file_remaining == 0UL)
    {
        status = (uint8_t)((s_file_crc ^ 0xFFFFFFFFUL) ==
                           s_file_crc_expected);
        font_download_show(status != 0U ? "FONT DOWNLOAD OK" :
                           "FONT FILE CRC ERROR");
        font_download_send(status != 0U ? 'D' : 'S');
        font_download_reset();
        return;
    }

    s_block_size = s_file_remaining > FONT_DOWNLOAD_BLOCK_SIZE ?
                   FONT_DOWNLOAD_BLOCK_SIZE : (uint16_t)s_file_remaining;
    s_block_index = 0U;
    s_state = FONT_DOWNLOAD_DATA;
}

void font_download_demo_init(void)
{
    board_lowlevel_init();
    delay_init();
    usart1_init(115200U);
    font_download_reset();
    lcd_init();
    lcd_set_text_color(LCD_BLUE);
    lcd_set_back_color(LCD_WHITE);
    lcd_set_window_clear(1U);
    lcd_show_string(20U, 20U, LCD_WIDTH - 40U, 24U,
                    "FONT DOWNLOAD DEMO", 16U);
    lcd_show_string(20U, 120U, 100U, 24U, "FILE:", 16U);
    font_download_show("WAITING FOR FONT...");
    s_flash_ready = (font_flash_init() == 0U &&
                     w25q128_read_id() == W25Q128_JEDEC_ID) ? 1U : 0U;

    font_download_show(s_flash_ready != 0U ? "FONT READY" :
                       "FONT FLASH ERROR");
    usart1_send_string("Font download ready\r\n");
    usart1_send_string("Send FONT header and file blocks\r\n");
    if (s_flash_ready == 0U)
    {
        usart1_send_string("W25Q128 not found\r\n");
    }
}

void font_download_demo_run(void)
{
    uint8_t data;

    if (s_flash_ready == 0U)
    {
        return;
    }

    /* 兜底同步 DMA，避免短包尚未触发 IDLE 时协议停在半包状态。 */
    usart1_poll();
    while (font_download_read_byte(&data) != 0U)
    {
        if (s_state == FONT_DOWNLOAD_IDLE)
        {
            if (data == (uint8_t)FONT_DOWNLOAD_MAGIC[s_magic_index])
            {
                s_magic_index++;
                if (s_magic_index == FONT_DOWNLOAD_MAGIC_SIZE)
                {
                    s_state = FONT_DOWNLOAD_HEADER;
                    s_header_index = 0U;
                    font_download_send('K');
                }
            }
            else
            {
                s_magic_index = data == (uint8_t)'F' ? 1U : 0U;
            }
        }
        else if (s_state == FONT_DOWNLOAD_HEADER)
        {
            s_header[s_header_index++] = data;
            if (s_header_index == sizeof(s_header))
            {
                (void)font_download_process_header();
            }
        }
        else if (s_state == FONT_DOWNLOAD_DATA)
        {
            s_block[s_block_index++] = data;
            if (s_block_index == s_block_size)
            {
                font_download_process_block();
            }
        }
    }
}
