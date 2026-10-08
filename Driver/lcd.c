#include "lcd.h"
#include "pwm.h"
#include "font.h"
#include "delay.h"
#include "stm32f10x_fsmc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

typedef struct
{
    volatile uint16_t reg;
    volatile uint16_t ram;
} lcd_map_t;

#define LCD_REG ((lcd_map_t *)LCD_BASE)

static uint16_t s_text_color = LCD_BLACK;
static uint16_t s_back_color = LCD_WHITE;
static uint8_t s_window_clear = 1U;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t write_ram_cmd;
    uint16_t set_x_cmd;
    uint16_t set_y_cmd;
} lcd_device_t;

static lcd_device_t s_lcd = {LCD_WIDTH, LCD_HEIGHT, 0x2C00U, 0x2A00U, 0x2B00U};

static void lcd_bus_init(void)
{
    GPIO_InitTypeDef gpio;
    FSMC_NORSRAMInitTypeDef fsmc;
    FSMC_NORSRAMTimingInitTypeDef read_timing;
    FSMC_NORSRAMTimingInitTypeDef write_timing;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOD |
                           RCC_APB2Periph_GPIOE |
                           RCC_APB2Periph_GPIOG |
                           RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
                    GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                    GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOD, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                    GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 |
                    GPIO_Pin_14 | GPIO_Pin_15;
    GPIO_Init(GPIOE, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_12;
    GPIO_Init(GPIOG, &gpio);

    /* Keep the backlight off as a GPIO while the controller is initialized. */
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = GPIO_Pin_0;
    GPIO_Init(GPIOB, &gpio);
    GPIO_ResetBits(GPIOB, GPIO_Pin_0);

#if LCD_USE_RESET
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = LCD_RESET_PIN;
    GPIO_Init(LCD_RESET_PORT, &gpio);
    GPIO_SetBits(LCD_RESET_PORT, LCD_RESET_PIN);
#endif

    fsmc.FSMC_ReadWriteTimingStruct = &read_timing;
    fsmc.FSMC_WriteTimingStruct = &write_timing;
    FSMC_NORSRAMStructInit(&fsmc);

    read_timing.FSMC_AddressSetupTime = 1U;
    read_timing.FSMC_DataSetupTime = 15U;
    write_timing.FSMC_AddressSetupTime = 0U;
    write_timing.FSMC_DataSetupTime = 3U;

    fsmc.FSMC_Bank = FSMC_Bank1_NORSRAM4;
    fsmc.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
    fsmc.FSMC_MemoryType = FSMC_MemoryType_SRAM;
    fsmc.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
    fsmc.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
    fsmc.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
    fsmc.FSMC_AsynchronousWait = FSMC_AsynchronousWait_Disable;
    fsmc.FSMC_WrapMode = FSMC_WrapMode_Disable;
    fsmc.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
    fsmc.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
    fsmc.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
    fsmc.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable;
    fsmc.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
    fsmc.FSMC_ReadWriteTimingStruct = &read_timing;
    fsmc.FSMC_WriteTimingStruct = &write_timing;
    FSMC_NORSRAMInit(&fsmc);
    FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM4, ENABLE);
}

static void lcd_write_reg(uint16_t reg)
{
    LCD_REG->reg = reg;
}

static void lcd_write_data(uint16_t data)
{
    LCD_REG->ram = data;
}

static void lcd_write_reg_data(uint16_t reg, uint16_t data)
{
    lcd_write_reg(reg);
    lcd_write_data(data);
}

static void lcd_reset(void)
{
#if LCD_USE_RESET
    GPIO_ResetBits(LCD_RESET_PORT, LCD_RESET_PIN);
    delay_ms(20U);
    GPIO_SetBits(LCD_RESET_PORT, LCD_RESET_PIN);
    delay_ms(120U);
#endif
}

typedef struct
{
    uint16_t reg;
    uint16_t value;
} lcd_init_command;

static const lcd_init_command lcd_init_cmds[] =
    {
        {0xF000, 0x0055},
        {0xF001, 0x00AA},
        {0xF002, 0x0052},
        {0xF003, 0x0008},
        {0xF004, 0x0001},
        {0xB000, 0x000D},
        {0xB001, 0x000D},
        {0xB002, 0x000D},
        {0xB600, 0x0034},
        {0xB601, 0x0034},
        {0xB602, 0x0034},
        {0xB100, 0x000D},
        {0xB101, 0x000D},
        {0xB102, 0x000D},
        {0xB700, 0x0034},
        {0xB701, 0x0034},
        {0xB702, 0x0034},
        {0xB200, 0x0000},
        {0xB201, 0x0000},
        {0xB202, 0x0000},
        {0xB800, 0x0024},
        {0xB801, 0x0024},
        {0xB802, 0x0024},
        {0xBF00, 0x0001},
        {0xB300, 0x000F},
        {0xB301, 0x000F},
        {0xB302, 0x000F},
        {0xB900, 0x0034},
        {0xB901, 0x0034},
        {0xB902, 0x0034},
        {0xB500, 0x0008},
        {0xB501, 0x0008},
        {0xB502, 0x0008},
        {0xC200, 0x0003},
        {0xBA00, 0x0024},
        {0xBA01, 0x0024},
        {0xBA02, 0x0024},
        {0xBC00, 0x0000},
        {0xBC01, 0x0078},
        {0xBC02, 0x0000},
        {0xBD00, 0x0000},
        {0xBD01, 0x0078},
        {0xBD02, 0x0000},
        {0xBE00, 0x0000},
        {0xBE01, 0x0064},
        {0xD100, 0x0000},
        {0xD101, 0x0033},
        {0xD102, 0x0000},
        {0xD103, 0x0034},
        {0xD104, 0x0000},
        {0xD105, 0x003A},
        {0xD106, 0x0000},
        {0xD107, 0x004A},
        {0xD108, 0x0000},
        {0xD109, 0x005C},
        {0xD10A, 0x0000},
        {0xD10B, 0x0081},
        {0xD10C, 0x0000},
        {0xD10D, 0x00A6},
        {0xD10E, 0x0000},
        {0xD10F, 0x00E5},
        {0xD110, 0x0001},
        {0xD111, 0x0013},
        {0xD112, 0x0001},
        {0xD113, 0x0054},
        {0xD114, 0x0001},
        {0xD115, 0x0082},
        {0xD116, 0x0001},
        {0xD117, 0x00CA},
        {0xD118, 0x0002},
        {0xD119, 0x0000},
        {0xD11A, 0x0002},
        {0xD11B, 0x0001},
        {0xD11C, 0x0002},
        {0xD11D, 0x0034},
        {0xD11E, 0x0002},
        {0xD11F, 0x0067},
        {0xD120, 0x0002},
        {0xD121, 0x0084},
        {0xD122, 0x0002},
        {0xD123, 0x00A4},
        {0xD124, 0x0002},
        {0xD125, 0x00B7},
        {0xD126, 0x0002},
        {0xD127, 0x00CF},
        {0xD128, 0x0002},
        {0xD129, 0x00DE},
        {0xD12A, 0x0002},
        {0xD12B, 0x00F2},
        {0xD12C, 0x0002},
        {0xD12D, 0x00FE},
        {0xD12E, 0x0003},
        {0xD12F, 0x0010},
        {0xD130, 0x0003},
        {0xD131, 0x0033},
        {0xD132, 0x0003},
        {0xD133, 0x006D},
        {0xD200, 0x0000},
        {0xD201, 0x0033},
        {0xD202, 0x0000},
        {0xD203, 0x0034},
        {0xD204, 0x0000},
        {0xD205, 0x003A},
        {0xD206, 0x0000},
        {0xD207, 0x004A},
        {0xD208, 0x0000},
        {0xD209, 0x005C},
        {0xD20A, 0x0000},
        {0xD20B, 0x0081},
        {0xD20C, 0x0000},
        {0xD20D, 0x00A6},
        {0xD20E, 0x0000},
        {0xD20F, 0x00E5},
        {0xD210, 0x0001},
        {0xD211, 0x0013},
        {0xD212, 0x0001},
        {0xD213, 0x0054},
        {0xD214, 0x0001},
        {0xD215, 0x0082},
        {0xD216, 0x0001},
        {0xD217, 0x00CA},
        {0xD218, 0x0002},
        {0xD219, 0x0000},
        {0xD21A, 0x0002},
        {0xD21B, 0x0001},
        {0xD21C, 0x0002},
        {0xD21D, 0x0034},
        {0xD21E, 0x0002},
        {0xD21F, 0x0067},
        {0xD220, 0x0002},
        {0xD221, 0x0084},
        {0xD222, 0x0002},
        {0xD223, 0x00A4},
        {0xD224, 0x0002},
        {0xD225, 0x00B7},
        {0xD226, 0x0002},
        {0xD227, 0x00CF},
        {0xD228, 0x0002},
        {0xD229, 0x00DE},
        {0xD22A, 0x0002},
        {0xD22B, 0x00F2},
        {0xD22C, 0x0002},
        {0xD22D, 0x00FE},
        {0xD22E, 0x0003},
        {0xD22F, 0x0010},
        {0xD230, 0x0003},
        {0xD231, 0x0033},
        {0xD232, 0x0003},
        {0xD233, 0x006D},
        {0xD300, 0x0000},
        {0xD301, 0x0033},
        {0xD302, 0x0000},
        {0xD303, 0x0034},
        {0xD304, 0x0000},
        {0xD305, 0x003A},
        {0xD306, 0x0000},
        {0xD307, 0x004A},
        {0xD308, 0x0000},
        {0xD309, 0x005C},
        {0xD30A, 0x0000},
        {0xD30B, 0x0081},
        {0xD30C, 0x0000},
        {0xD30D, 0x00A6},
        {0xD30E, 0x0000},
        {0xD30F, 0x00E5},
        {0xD310, 0x0001},
        {0xD311, 0x0013},
        {0xD312, 0x0001},
        {0xD313, 0x0054},
        {0xD314, 0x0001},
        {0xD315, 0x0082},
        {0xD316, 0x0001},
        {0xD317, 0x00CA},
        {0xD318, 0x0002},
        {0xD319, 0x0000},
        {0xD31A, 0x0002},
        {0xD31B, 0x0001},
        {0xD31C, 0x0002},
        {0xD31D, 0x0034},
        {0xD31E, 0x0002},
        {0xD31F, 0x0067},
        {0xD320, 0x0002},
        {0xD321, 0x0084},
        {0xD322, 0x0002},
        {0xD323, 0x00A4},
        {0xD324, 0x0002},
        {0xD325, 0x00B7},
        {0xD326, 0x0002},
        {0xD327, 0x00CF},
        {0xD328, 0x0002},
        {0xD329, 0x00DE},
        {0xD32A, 0x0002},
        {0xD32B, 0x00F2},
        {0xD32C, 0x0002},
        {0xD32D, 0x00FE},
        {0xD32E, 0x0003},
        {0xD32F, 0x0010},
        {0xD330, 0x0003},
        {0xD331, 0x0033},
        {0xD332, 0x0003},
        {0xD333, 0x006D},
        {0xD400, 0x0000},
        {0xD401, 0x0033},
        {0xD402, 0x0000},
        {0xD403, 0x0034},
        {0xD404, 0x0000},
        {0xD405, 0x003A},
        {0xD406, 0x0000},
        {0xD407, 0x004A},
        {0xD408, 0x0000},
        {0xD409, 0x005C},
        {0xD40A, 0x0000},
        {0xD40B, 0x0081},
        {0xD40C, 0x0000},
        {0xD40D, 0x00A6},
        {0xD40E, 0x0000},
        {0xD40F, 0x00E5},
        {0xD410, 0x0001},
        {0xD411, 0x0013},
        {0xD412, 0x0001},
        {0xD413, 0x0054},
        {0xD414, 0x0001},
        {0xD415, 0x0082},
        {0xD416, 0x0001},
        {0xD417, 0x00CA},
        {0xD418, 0x0002},
        {0xD419, 0x0000},
        {0xD41A, 0x0002},
        {0xD41B, 0x0001},
        {0xD41C, 0x0002},
        {0xD41D, 0x0034},
        {0xD41E, 0x0002},
        {0xD41F, 0x0067},
        {0xD420, 0x0002},
        {0xD421, 0x0084},
        {0xD422, 0x0002},
        {0xD423, 0x00A4},
        {0xD424, 0x0002},
        {0xD425, 0x00B7},
        {0xD426, 0x0002},
        {0xD427, 0x00CF},
        {0xD428, 0x0002},
        {0xD429, 0x00DE},
        {0xD42A, 0x0002},
        {0xD42B, 0x00F2},
        {0xD42C, 0x0002},
        {0xD42D, 0x00FE},
        {0xD42E, 0x0003},
        {0xD42F, 0x0010},
        {0xD430, 0x0003},
        {0xD431, 0x0033},
        {0xD432, 0x0003},
        {0xD433, 0x006D},
        {0xD500, 0x0000},
        {0xD501, 0x0033},
        {0xD502, 0x0000},
        {0xD503, 0x0034},
        {0xD504, 0x0000},
        {0xD505, 0x003A},
        {0xD506, 0x0000},
        {0xD507, 0x004A},
        {0xD508, 0x0000},
        {0xD509, 0x005C},
        {0xD50A, 0x0000},
        {0xD50B, 0x0081},
        {0xD50C, 0x0000},
        {0xD50D, 0x00A6},
        {0xD50E, 0x0000},
        {0xD50F, 0x00E5},
        {0xD510, 0x0001},
        {0xD511, 0x0013},
        {0xD512, 0x0001},
        {0xD513, 0x0054},
        {0xD514, 0x0001},
        {0xD515, 0x0082},
        {0xD516, 0x0001},
        {0xD517, 0x00CA},
        {0xD518, 0x0002},
        {0xD519, 0x0000},
        {0xD51A, 0x0002},
        {0xD51B, 0x0001},
        {0xD51C, 0x0002},
        {0xD51D, 0x0034},
        {0xD51E, 0x0002},
        {0xD51F, 0x0067},
        {0xD520, 0x0002},
        {0xD521, 0x0084},
        {0xD522, 0x0002},
        {0xD523, 0x00A4},
        {0xD524, 0x0002},
        {0xD525, 0x00B7},
        {0xD526, 0x0002},
        {0xD527, 0x00CF},
        {0xD528, 0x0002},
        {0xD529, 0x00DE},
        {0xD52A, 0x0002},
        {0xD52B, 0x00F2},
        {0xD52C, 0x0002},
        {0xD52D, 0x00FE},
        {0xD52E, 0x0003},
        {0xD52F, 0x0010},
        {0xD530, 0x0003},
        {0xD531, 0x0033},
        {0xD532, 0x0003},
        {0xD533, 0x006D},
        {0xD600, 0x0000},
        {0xD601, 0x0033},
        {0xD602, 0x0000},
        {0xD603, 0x0034},
        {0xD604, 0x0000},
        {0xD605, 0x003A},
        {0xD606, 0x0000},
        {0xD607, 0x004A},
        {0xD608, 0x0000},
        {0xD609, 0x005C},
        {0xD60A, 0x0000},
        {0xD60B, 0x0081},
        {0xD60C, 0x0000},
        {0xD60D, 0x00A6},
        {0xD60E, 0x0000},
        {0xD60F, 0x00E5},
        {0xD610, 0x0001},
        {0xD611, 0x0013},
        {0xD612, 0x0001},
        {0xD613, 0x0054},
        {0xD614, 0x0001},
        {0xD615, 0x0082},
        {0xD616, 0x0001},
        {0xD617, 0x00CA},
        {0xD618, 0x0002},
        {0xD619, 0x0000},
        {0xD61A, 0x0002},
        {0xD61B, 0x0001},
        {0xD61C, 0x0002},
        {0xD61D, 0x0034},
        {0xD61E, 0x0002},
        {0xD61F, 0x0067},
        {0xD620, 0x0002},
        {0xD621, 0x0084},
        {0xD622, 0x0002},
        {0xD623, 0x00A4},
        {0xD624, 0x0002},
        {0xD625, 0x00B7},
        {0xD626, 0x0002},
        {0xD627, 0x00CF},
        {0xD628, 0x0002},
        {0xD629, 0x00DE},
        {0xD62A, 0x0002},
        {0xD62B, 0x00F2},
        {0xD62C, 0x0002},
        {0xD62D, 0x00FE},
        {0xD62E, 0x0003},
        {0xD62F, 0x0010},
        {0xD630, 0x0003},
        {0xD631, 0x0033},
        {0xD632, 0x0003},
        {0xD633, 0x006D},
        {0xF000, 0x0055},
        {0xF001, 0x00AA},
        {0xF002, 0x0052},
        {0xF003, 0x0008},
        {0xF004, 0x0000},
        {0xB100, 0x00CC},
        {0xB101, 0x0000},
        {0xB600, 0x0005},
        {0xB700, 0x0070},
        {0xB701, 0x0070},
        {0xB800, 0x0001},
        {0xB801, 0x0003},
        {0xB802, 0x0003},
        {0xB803, 0x0003},
        {0xBC00, 0x0002},
        {0xBC01, 0x0000},
        {0xBC02, 0x0000},
        {0xC900, 0x00D0},
        {0xC901, 0x0002},
        {0xC902, 0x0050},
        {0xC903, 0x0050},
        {0xC904, 0x0050},
        {0x3500, 0x0000},
        {0x3A00, 0x0055},
};

static void lcd_write_init_sequence(void)
{
    uint32_t i;

    for (i = 0U; i < (sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0])); i++)
    {
        lcd_write_reg_data(lcd_init_cmds[i].reg, lcd_init_cmds[i].value);
    }
}

static void lcd_set_scan_direction(void)
{
    /* NT5510 portrait 480x800 scan direction, matching the reference driver. */
    lcd_write_reg_data(0x3600U, 0x0000U);
    lcd_write_reg(0x2A00U);
    lcd_write_data(0x0000U);
    lcd_write_reg(0x2A01U);
    lcd_write_data(0x0000U);
    lcd_write_reg(0x2A02U);
    lcd_write_data((s_lcd.width - 1U) >> 8);
    lcd_write_reg(0x2A03U);
    lcd_write_data((s_lcd.width - 1U) & 0xFFU);
    lcd_write_reg(0x2B00U);
    lcd_write_data(0x0000U);
    lcd_write_reg(0x2B01U);
    lcd_write_data(0x0000U);
    lcd_write_reg(0x2B02U);
    lcd_write_data((s_lcd.height - 1U) >> 8);
    lcd_write_reg(0x2B03U);
    lcd_write_data((s_lcd.height - 1U) & 0xFFU);
}

void lcd_set_cursor(uint16_t x, uint16_t y)
{
    if (x >= s_lcd.width)
    {
        x = s_lcd.width - 1U;
    }
    if (y >= s_lcd.height)
    {
        y = s_lcd.height - 1U;
    }

    lcd_write_reg(0x2A00U);
    lcd_write_data(x >> 8);
    lcd_write_reg(0x2A01U);
    lcd_write_data(x & 0xFFU);
    lcd_write_reg(0x2B00U);
    lcd_write_data(y >> 8);
    lcd_write_reg(0x2B01U);
    lcd_write_data(y & 0xFFU);
}

void lcd_set_window(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    uint16_t x_end;
    uint16_t y_end;

    if (width == 0U || height == 0U || x >= s_lcd.width || y >= s_lcd.height)
    {
        return;
    }
    if (width > s_lcd.width - x)
    {
        width = s_lcd.width - x;
    }
    if (height > s_lcd.height - y)
    {
        height = s_lcd.height - y;
    }

    x_end = (uint16_t)(x + width - 1U);
    y_end = (uint16_t)(y + height - 1U);

    lcd_write_reg(0x2A00U);
    lcd_write_data(x >> 8);
    lcd_write_reg(0x2A01U);
    lcd_write_data(x & 0xFFU);
    lcd_write_reg(0x2A02U);
    lcd_write_data(x_end >> 8);
    lcd_write_reg(0x2A03U);
    lcd_write_data(x_end & 0xFFU);
    lcd_write_reg(0x2B00U);
    lcd_write_data(y >> 8);
    lcd_write_reg(0x2B01U);
    lcd_write_data(y & 0xFFU);
    lcd_write_reg(0x2B02U);
    lcd_write_data(y_end >> 8);
    lcd_write_reg(0x2B03U);
    lcd_write_data(y_end & 0xFFU);
}

static void lcd_start_write(void)
{
    lcd_write_reg(s_lcd.write_ram_cmd);
}

void lcd_clear(uint16_t color)
{
    uint32_t total;
    uint32_t i;

    lcd_set_window(0U, 0U, s_lcd.width, s_lcd.height);
    lcd_start_write();
    total = (uint32_t)s_lcd.width * s_lcd.height;
    for (i = 0U; i < total; i++)
    {
        LCD_REG->ram = color;
    }
}

void lcd_fill(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
              uint16_t color)
{
    uint32_t total;
    uint32_t i;

    lcd_set_window(x, y, width, height);
    if (width == 0U || height == 0U ||
        x >= s_lcd.width || y >= s_lcd.height)
    {
        return;
    }
    if (width > s_lcd.width - x)
    {
        width = s_lcd.width - x;
    }
    if (height > s_lcd.height - y)
    {
        height = s_lcd.height - y;
    }

    lcd_start_write();
    total = (uint32_t)width * height;
    for (i = 0U; i < total; i++)
    {
        LCD_REG->ram = color;
    }
}

void lcd_draw_point(uint16_t x, uint16_t y)
{
    if (x >= s_lcd.width || y >= s_lcd.height)
    {
        return;
    }
    lcd_set_cursor(x, y);
    lcd_start_write();
    LCD_REG->ram = s_text_color;
}

static void lcd_draw_glyph(uint16_t x, uint16_t y, uint8_t data, uint8_t size)
{
    const uint8_t *glyph;
    uint8_t width;
    uint8_t bytes;
    uint8_t row;
    uint8_t column;

    if (x >= s_lcd.width || y >= s_lcd.height ||
        size == 0U || x > s_lcd.width - size / 2U ||
        y > s_lcd.height - size)
    {
        return;
    }

    glyph = font_get_ascii_glyph(data, size, &width, &bytes);
    if (glyph == 0)
    {
        return;
    }

    lcd_set_window(x, y, width, size);
    lcd_start_write();
    for (row = 0U; row < size; row++)
    {
        for (column = 0U; column < width; column++)
        {
            if ((glyph[column * bytes + row / 8U] &
                 (uint8_t)(0x80U >> (row % 8U))) != 0U)
            {
                LCD_REG->ram = s_text_color;
            }
            else
            {
                LCD_REG->ram = s_window_clear ? s_back_color : s_text_color;
            }
        }
    }
}

static void lcd_draw_font_glyph(uint16_t x, uint16_t y, uint8_t size,
                                const uint8_t *glyph)
{
    uint8_t bytes;
    uint8_t row;
    uint8_t column;

    if (glyph == 0 || size == 0U || x >= s_lcd.width || y >= s_lcd.height ||
        x > s_lcd.width - size || y > s_lcd.height - size)
    {
        return;
    }

    bytes = (uint8_t)((size + 7U) / 8U);
    lcd_set_window(x, y, size, size);
    lcd_start_write();
    for (row = 0U; row < size; row++)
    {
        for (column = 0U; column < size; column++)
        {
            if ((glyph[column * bytes + row / 8U] &
                 (uint8_t)(0x80U >> (row % 8U))) != 0U)
            {
                LCD_REG->ram = s_text_color;
            }
            else
            {
                LCD_REG->ram = s_window_clear ? s_back_color : s_text_color;
            }
        }
    }
}

void lcd_show_char(uint16_t x, uint16_t y, uint8_t data, uint8_t size)
{
    if (data < ' ' || data > '~')
    {
        data = '?';
    }
    lcd_draw_glyph(x, y, data, size);
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t number, uint8_t length,
                  uint8_t size)
{
    uint32_t divisor = 1U;
    uint8_t i;
    uint8_t digit;
    uint8_t started = 0U;

    if (length == 0U)
    {
        return;
    }
    for (i = 1U; i < length; i++)
    {
        divisor *= 10U;
    }
    for (i = 0U; i < length; i++)
    {
        digit = (uint8_t)((number / divisor) % 10U);
        if (started == 0U && i < length - 1U && digit == 0U)
        {
            lcd_show_char((uint16_t)(x + (size / 2U) * i), y, ' ', size);
        }
        else
        {
            started = 1U;
            lcd_show_char((uint16_t)(x + (size / 2U) * i), y,
                          (uint8_t)('0' + digit), size);
        }
        divisor /= 10U;
    }
}

void lcd_show_signed_num(uint16_t x, uint16_t y, int32_t number,
                         uint8_t length, uint8_t size)
{
    if (length == 0U)
    {
        return;
    }
    if (number < 0)
    {
        lcd_show_char(x, y, '-', size);
        lcd_show_num((uint16_t)(x + size / 2U), y,
                     (uint32_t)(-(number + 1)) + 1U, length - 1U, size);
    }
    else
    {
        lcd_show_num(x, y, (uint32_t)number, length, size);
    }
}

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     const char *text, uint8_t size)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    uint16_t right = (uint16_t)(x + width);
    uint16_t bottom = (uint16_t)(y + height);
    uint8_t count;
    uint8_t char_width;
    uint8_t glyph[72];
    uint32_t code;

    if (text == 0 || width == 0U || height == 0U)
    {
        return;
    }
    if (s_window_clear != 0U)
    {
        lcd_fill(x, y, width, height, s_back_color);
    }

    while (*text != 0)
    {
        count = font_decode_text((const uint8_t *)text, &code);
        if (count == 0U)
        {
            break;
        }
        text += count;

        if (code == '\r')
        {
            continue;
        }
        if (code == '\n')
        {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + size);
            continue;
        }
        char_width = font_get_char_width(code, size);
        if (cursor_x != x && cursor_x + char_width > right)
        {
            cursor_x = x;
            cursor_y = (uint16_t)(cursor_y + size);
        }
        if (cursor_y + size > bottom)
        {
            break;
        }

        if (code <= 0x7FU)
        {
            lcd_show_char(cursor_x, cursor_y, (uint8_t)code, size);
        }
        else if (font_read_glyph(code, size, glyph) == 0U)
        {
            lcd_draw_font_glyph(cursor_x, cursor_y, size, glyph);
        }
        else
        {
            lcd_show_char(cursor_x, cursor_y, '?', size);
        }
        cursor_x = (uint16_t)(cursor_x + char_width);
    }
}

void lcd_set_text_color(uint16_t color)
{
    s_text_color = color;
}

void lcd_set_back_color(uint16_t color)
{
    s_back_color = color;
}

void lcd_set_window_clear(uint8_t enable)
{
    s_window_clear = enable != 0U;
}

void lcd_set_backlight(uint8_t percent)
{
    pwm_set_percent(percent);
}

void lcd_display_on(void)
{
    lcd_write_reg(0x2900U);
}

void lcd_display_off(void)
{
    lcd_write_reg(0x2800U);
}

void lcd_init(void)
{
    lcd_bus_init();
    lcd_reset();
    delay_ms(50U);
    lcd_write_init_sequence();
    lcd_write_reg(0x1100U);
    delay_ms(120U);
    lcd_write_reg(0x2900U);
    lcd_set_scan_direction();
    lcd_set_text_color(LCD_BLACK);
    lcd_set_back_color(LCD_WHITE);
    lcd_set_window_clear(1U);
    pwm_init();
    font_flash_init();
    lcd_set_backlight(50U);
    lcd_clear(LCD_WHITE);
}
