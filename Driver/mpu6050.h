#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

#define MPU6050_I2C_ADDRESS 0x68U

typedef struct
{
    int16_t Acc_x;
    int16_t Acc_y;
    int16_t Acc_z;
    int16_t Temperature;
    int16_t Gyro_x;
    int16_t Gyro_y;
    int16_t Gyro_z;
} mpu6050_data_t;

uint8_t mpu6050_init(void);
uint8_t mpu6050_write_reg(uint8_t reg, uint8_t data);
uint8_t mpu6050_read_reg(uint8_t reg, uint8_t *data);
uint8_t mpu6050_read_id(uint8_t *id);
uint8_t mpu6050_read_data(mpu6050_data_t *data);

#endif /* __MPU6050_H */
