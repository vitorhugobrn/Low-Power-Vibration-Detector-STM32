/*
 * mpu6050.h
 *
 *  Created on: May 26, 2026
 *      Author: Aluno
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

#define MPU_ADDR		 0x68
#define REG_PWR_MGMT_1	 0x6B
#define REG_PWR_MGMT_2	 0x6C
#define REG_ACCEL_XOUT   0x3B
#define REG_GYRO_CONFIG  0x1B
#define REG_ACCEL_CONFIG 0x1C
#define REG_SMPLRT_DIV	 0x19
#define REG_CONFIG		 0x1A

typedef struct {
	I2C_HandleTypeDef *hi2c;
	uint8_t addr;
} MPU6050;

HAL_StatusTypeDef mpu_write(MPU6050 *mod, uint8_t reg, uint8_t val);
HAL_StatusTypeDef mpu_read(MPU6050 *mod, uint8_t reg, uint8_t *buf, uint8_t len);
void mpu_wake(MPU6050 *mod);
void mpu_sleep(MPU6050 *mod);
HAL_StatusTypeDef mpu_read_accel(MPU6050 *mod, int16_t *ax, int16_t *ay, int16_t *az);

#endif /* INC_MPU6050_H_ */
