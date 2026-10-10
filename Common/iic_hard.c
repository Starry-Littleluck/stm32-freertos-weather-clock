#include "iic_hard.h"
#include "delay.h"
#include "stm32f10x_gpio.h"

static volatile uint8_t s_debug_stage;
static volatile uint16_t s_debug_sr1;
static volatile uint16_t s_debug_sr2;
static volatile uint16_t s_debug_cr1;
static volatile uint16_t s_debug_cr2;

static void iic_hard_save_debug(I2C_TypeDef *instance, uint8_t stage)
{
    s_debug_stage = stage;
    s_debug_sr1 = instance != 0 ? instance->SR1 : 0U;
    s_debug_sr2 = instance != 0 ? instance->SR2 : 0U;
    s_debug_cr1 = instance != 0 ? instance->CR1 : 0U;
    s_debug_cr2 = instance != 0 ? instance->CR2 : 0U;
}

static uint8_t iic_hard_check_error(const iic_hard_desc_t iic)
{
    if (I2C_GetFlagStatus(iic->Instance, I2C_FLAG_AF) != RESET)
    {
        I2C_ClearFlag(iic->Instance, I2C_FLAG_AF);
        return 2U;
    }
    if (I2C_GetFlagStatus(iic->Instance, I2C_FLAG_BERR) != RESET ||
        I2C_GetFlagStatus(iic->Instance, I2C_FLAG_ARLO) != RESET ||
        I2C_GetFlagStatus(iic->Instance, I2C_FLAG_OVR) != RESET)
    {
        I2C_ClearFlag(iic->Instance, I2C_FLAG_BERR | I2C_FLAG_ARLO |
                      I2C_FLAG_OVR);
        return 3U;
    }
    return 0U;
}

static uint8_t iic_hard_wait_flag(const iic_hard_desc_t iic, uint32_t flag,
                                  uint8_t stage)
{
    uint32_t timeout = iic->Timeout;
    uint8_t status;

    while (I2C_GetFlagStatus(iic->Instance, flag) == RESET)
    {
        status = iic_hard_check_error(iic);
        if (status != 0U)
        {
            iic_hard_save_debug(iic->Instance, stage);
            return status;
        }
        if (timeout == 0U)
        {
            iic_hard_save_debug(iic->Instance, stage);
            return 3U;
        }
        timeout--;
    }
    status = iic_hard_check_error(iic);
    if (status != 0U)
        iic_hard_save_debug(iic->Instance, stage);
    return status;
}

static void iic_hard_clear_addr(const iic_hard_desc_t iic)
{
    (void)iic->Instance->SR1;
    (void)iic->Instance->SR2;
}

static void iic_hard_recover_bus(const iic_hard_desc_t iic)
{
    GPIO_InitTypeDef gpio;
    uint8_t pulse;

    gpio.GPIO_Pin = iic->Pin_scl | iic->Pin_sda;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(iic->Gpio_port, &gpio);
    GPIO_SetBits(iic->Gpio_port, gpio.GPIO_Pin);
    delay_us(5U);

    /* 如果从机在复位时保持 SDA，输出最多 9 个 SCL 脉冲释放它。 */
    for (pulse = 0U; pulse < 9U; pulse++)
    {
        if (GPIO_ReadInputDataBit(iic->Gpio_port, iic->Pin_sda) != Bit_RESET)
            break;
        GPIO_ResetBits(iic->Gpio_port, iic->Pin_scl);
        delay_us(5U);
        GPIO_SetBits(iic->Gpio_port, iic->Pin_scl);
        delay_us(5U);
    }

    /* SDA 低 -> SCL 高 -> SDA 高，构造一个总线 STOP。 */
    GPIO_ResetBits(iic->Gpio_port, iic->Pin_sda);
    delay_us(5U);
    GPIO_SetBits(iic->Gpio_port, iic->Pin_scl);
    delay_us(5U);
    GPIO_SetBits(iic->Gpio_port, iic->Pin_sda);
    delay_us(5U);

    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(iic->Gpio_port, &gpio);
    GPIO_SetBits(iic->Gpio_port, gpio.GPIO_Pin);
}

static void iic_hard_abort(const iic_hard_desc_t iic)
{
    I2C_GenerateSTOP(iic->Instance, ENABLE);
    I2C_AcknowledgeConfig(iic->Instance, ENABLE);
    I2C_Cmd(iic->Instance, DISABLE);
    iic_hard_recover_bus(iic);
    I2C_Cmd(iic->Instance, ENABLE);
}

static uint8_t iic_hard_start(const iic_hard_desc_t iic, uint8_t address,
                              uint8_t direction)
{
    uint8_t status;

    I2C_GenerateSTART(iic->Instance, ENABLE);
    status = iic_hard_wait_flag(iic, I2C_FLAG_SB, 1U);
    if (status != 0U)
        return status;

    I2C_Send7bitAddress(iic->Instance, (uint8_t)(address << 1), direction);
    status = iic_hard_wait_flag(iic, I2C_FLAG_ADDR, 2U);
    if (status != 0U)
        return status;

    /* 接收方向保留 ADDR，调用者先设置 ACK/POS，再清除 ADDR。 */
    if (direction == I2C_Direction_Transmitter)
        iic_hard_clear_addr(iic);
    return 0U;
}

static uint8_t iic_hard_write_data(const iic_hard_desc_t iic,
                                   const uint8_t *data, uint16_t length)
{
    uint16_t index;
    uint8_t status;

    for (index = 0U; index < length; index++)
    {
        I2C_SendData(iic->Instance, data[index]);
        status = iic_hard_wait_flag(iic, I2C_FLAG_BTF, 3U);
        if (status != 0U)
            return status;
    }
    return 0U;
}

static uint8_t iic_hard_read_data(const iic_hard_desc_t iic, uint8_t *data,
                                  uint16_t length)
{
    uint16_t index;
    uint8_t status;

    if (length == 1U)
    {
        I2C_AcknowledgeConfig(iic->Instance, DISABLE);
        I2C_NACKPositionConfig(iic->Instance, I2C_NACKPosition_Current);
        iic_hard_clear_addr(iic);
        I2C_GenerateSTOP(iic->Instance, ENABLE);
        status = iic_hard_wait_flag(iic, I2C_FLAG_RXNE, 4U);
        if (status == 0U)
            data[0] = I2C_ReceiveData(iic->Instance);
        I2C_AcknowledgeConfig(iic->Instance, ENABLE);
        return status;
    }

    if (length == 2U)
    {
        I2C_AcknowledgeConfig(iic->Instance, DISABLE);
        I2C_NACKPositionConfig(iic->Instance, I2C_NACKPosition_Next);
        iic_hard_clear_addr(iic);
        status = iic_hard_wait_flag(iic, I2C_FLAG_BTF, 5U);
        if (status == 0U)
        {
            I2C_GenerateSTOP(iic->Instance, ENABLE);
            data[0] = I2C_ReceiveData(iic->Instance);
            data[1] = I2C_ReceiveData(iic->Instance);
        }
        I2C_NACKPositionConfig(iic->Instance, I2C_NACKPosition_Current);
        I2C_AcknowledgeConfig(iic->Instance, ENABLE);
        return status;
    }

    I2C_AcknowledgeConfig(iic->Instance, ENABLE);
    I2C_NACKPositionConfig(iic->Instance, I2C_NACKPosition_Current);
    iic_hard_clear_addr(iic);

    /* STM32F1 requires a dedicated ending sequence for N >= 3 bytes.
       ACK must be cleared while three bytes remain, otherwise the controller
       acknowledges one byte too many and can leave BUSY set. */
    for (index = 0U; index < (uint16_t)(length - 3U); index++)
    {
        status = iic_hard_wait_flag(iic, I2C_FLAG_RXNE, 6U);
        if (status != 0U)
            return status;
        data[index] = I2C_ReceiveData(iic->Instance);
    }

    status = iic_hard_wait_flag(iic, I2C_FLAG_BTF, 7U);
    if (status != 0U)
        return status;

    I2C_AcknowledgeConfig(iic->Instance, DISABLE);
    data[index++] = I2C_ReceiveData(iic->Instance);
    I2C_GenerateSTOP(iic->Instance, ENABLE);
    data[index++] = I2C_ReceiveData(iic->Instance);

    status = iic_hard_wait_flag(iic, I2C_FLAG_RXNE, 8U);
    if (status == 0U)
        data[index] = I2C_ReceiveData(iic->Instance);

    I2C_AcknowledgeConfig(iic->Instance, ENABLE);
    return status;
}

uint8_t iic_hard_init(iic_hard_desc_t iic)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef config;

    if (iic == 0 || iic->Instance == 0 || iic->Gpio_port == 0 ||
        iic->Pin_scl == 0U || iic->Pin_sda == 0U)
        return 1U;

    s_debug_stage = 0U;
    s_debug_sr1 = 0U;
    s_debug_sr2 = 0U;
    s_debug_cr1 = 0U;
    s_debug_cr2 = 0U;

    gpio.GPIO_Pin = iic->Pin_scl | iic->Pin_sda;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_SetBits(iic->Gpio_port, gpio.GPIO_Pin);
    GPIO_Init(iic->Gpio_port, &gpio);
    GPIO_SetBits(iic->Gpio_port, gpio.GPIO_Pin);

    if (iic->Instance == I2C1)
    {
        GPIO_PinRemapConfig(GPIO_Remap_I2C1, DISABLE);
        /* I2C1 默认使用 PB6/PB7；直接清位，避免复用状态残留。 */
        AFIO->MAPR &= ~(uint32_t)GPIO_Remap_I2C1;
    }

    iic_hard_recover_bus(iic);

    if (GPIO_ReadInputDataBit(iic->Gpio_port, iic->Pin_scl) == Bit_RESET ||
        GPIO_ReadInputDataBit(iic->Gpio_port, iic->Pin_sda) == Bit_RESET)
        return 3U;

    I2C_DeInit(iic->Instance);
    I2C_StructInit(&config);
    config.I2C_ClockSpeed = iic->Speed != 0U ? iic->Speed : 100000U;
    config.I2C_Mode = I2C_Mode_I2C;
    config.I2C_DutyCycle = I2C_DutyCycle_2;
    config.I2C_OwnAddress1 = 0U;
    config.I2C_Ack = I2C_Ack_Enable;
    config.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(iic->Instance, &config);
    I2C_Cmd(iic->Instance, ENABLE);
    return 0U;
}

uint8_t iic_hard_write(iic_hard_desc_t iic, uint8_t address,
                       const uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;

    status = iic_hard_start(iic, address, I2C_Direction_Transmitter);
    if (status == 0U)
        status = iic_hard_write_data(iic, data, length);
    if (status == 0U)
        I2C_GenerateSTOP(iic->Instance, ENABLE);
    else
        iic_hard_abort(iic);
    return status;
}

uint8_t iic_hard_write_reg(iic_hard_desc_t iic, uint8_t address, uint8_t reg,
                           const uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;

    status = iic_hard_start(iic, address, I2C_Direction_Transmitter);
    if (status == 0U)
        status = iic_hard_write_data(iic, &reg, 1U);
    if (status == 0U)
        status = iic_hard_write_data(iic, data, length);
    if (status == 0U)
        I2C_GenerateSTOP(iic->Instance, ENABLE);
    else
        iic_hard_abort(iic);
    return status;
}

uint8_t iic_hard_read_reg(iic_hard_desc_t iic, uint8_t address, uint8_t reg,
                          uint8_t *data, uint16_t length)
{
    uint8_t status;

    if (iic == 0 || (data == 0 && length != 0U))
        return 1U;
    if (length == 0U)
        return 0U;

    status = iic_hard_start(iic, address, I2C_Direction_Transmitter);
    if (status == 0U)
        status = iic_hard_write_data(iic, &reg, 1U);
    if (status == 0U)
        status = iic_hard_start(iic, address, I2C_Direction_Receiver);
    if (status == 0U)
        status = iic_hard_read_data(iic, data, length);

    I2C_NACKPositionConfig(iic->Instance, I2C_NACKPosition_Current);
    I2C_AcknowledgeConfig(iic->Instance, ENABLE);
    if (status == 0U)
        I2C_GenerateSTOP(iic->Instance, ENABLE);
    else
        iic_hard_abort(iic);
    return status;
}

void iic_hard_get_debug(uint8_t *stage, uint16_t *sr1, uint16_t *sr2,
                        uint16_t *cr1, uint16_t *cr2)
{
    if (stage != 0)
        *stage = s_debug_stage;
    if (sr1 != 0)
        *sr1 = s_debug_sr1;
    if (sr2 != 0)
        *sr2 = s_debug_sr2;
    if (cr1 != 0)
        *cr1 = s_debug_cr1;
    if (cr2 != 0)
        *cr2 = s_debug_cr2;
}
