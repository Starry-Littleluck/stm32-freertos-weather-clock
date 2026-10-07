#include "mpu6050.h"

#include "delay.h"
#include "iic_hard.h"
#include "mpu6050_reg.h"

static struct iic_hard_desc s_mpu6050_iic =
{
    .Instance = I2C2,
    .Gpio_port = GPIOB,
    .Pin_scl = GPIO_Pin_10,
    .Pin_sda = GPIO_Pin_11,
    .Speed = 100000U,
    .Timeout = 100000U
};

static uint8_t mpu6050_read_bytes(uint8_t reg, uint8_t *data,uint16_t length)
{
    return iic_hard_read_reg(&s_mpu6050_iic, MPU6050_I2C_ADDRESS,
                             reg, data, length);
}

uint8_t mpu6050_init(void)
{
    static const uint8_t config[][2] =
    {
        {MPU6050_PWR_MGMT_1, 0x01U},
        {MPU6050_PWR_MGMT_2, 0x00U},
        {MPU6050_SMPLRT_DIV, 0x09U},
        {MPU6050_CONFIG, 0x06U},
        {MPU6050_GYRO_CONFIG, 0x18U},
        {MPU6050_ACCEL_CONFIG, 0x18U}
    };
    uint8_t index;
    uint8_t status;

    status = iic_hard_init(&s_mpu6050_iic);
    if (status != 0U)
        return status;

    for (index = 0U; index < sizeof(config) / sizeof(config[0]); index++)
    {
        status = mpu6050_write_reg(config[index][0], config[index][1]);
        if (status != 0U)
            return status;
    }
    delay_ms(10U);
    return 0U;
}

uint8_t mpu6050_write_reg(uint8_t reg, uint8_t data)
{
    return iic_hard_write_reg(&s_mpu6050_iic, MPU6050_I2C_ADDRESS,
                              reg, &data, 1U);
}

uint8_t mpu6050_read_reg(uint8_t reg, uint8_t *data)
{
    if (data == 0)
        return 1U;
    return mpu6050_read_bytes(reg, data, 1U);
}

uint8_t mpu6050_read_id(uint8_t *id)
{
    return mpu6050_read_reg(MPU6050_WHO_AM_I, id);
}

uint8_t mpu6050_read_data(mpu6050_data_t *data)
{
    uint8_t buffer[14];
    uint8_t status;

    if (data == 0)
        return 1U;

    status = mpu6050_read_bytes(MPU6050_ACCEL_XOUT_H, buffer,
                                 sizeof(buffer));
    if (status != 0U)
        return status;

    data->Acc_x = (int16_t)(((uint16_t)buffer[0] << 8) | buffer[1]);
    data->Acc_y = (int16_t)(((uint16_t)buffer[2] << 8) | buffer[3]);
    data->Acc_z = (int16_t)(((uint16_t)buffer[4] << 8) | buffer[5]);
    data->Temperature = (int16_t)(((uint16_t)buffer[6] << 8) | buffer[7]);
    data->Gyro_x = (int16_t)(((uint16_t)buffer[8] << 8) | buffer[9]);
    data->Gyro_y = (int16_t)(((uint16_t)buffer[10] << 8) | buffer[11]);
    data->Gyro_z = (int16_t)(((uint16_t)buffer[12] << 8) | buffer[13]);
    return 0U;
}
