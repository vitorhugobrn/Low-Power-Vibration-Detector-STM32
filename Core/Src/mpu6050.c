/*
 * mpu6050.c
 *
 *  Created on: May 26, 2026
 *      Author: Aluno
 */

#include "mpu6050.h"

HAL_StatusTypeDef mpu_write(MPU6050 *mod, uint8_t reg, uint8_t val) {
	uint8_t buf[2] = { reg, val };
	return HAL_I2C_Master_Transmit(mod->hi2c, mod->addr << 1, buf, 2, 10);
 }

 HAL_StatusTypeDef mpu_read(MPU6050 *mod, uint8_t reg, uint8_t *buf, uint8_t len) {
     HAL_StatusTypeDef status;
     status = HAL_I2C_Master_Transmit(mod->hi2c, mod->addr << 1, &reg, 1, 10);
     if (status != HAL_OK) return status;
     return HAL_I2C_Master_Receive(mod->hi2c, mod->addr << 1, buf, len, 10);
 }

 void mpu_wake(MPU6050 *mod) {
     mpu_write(mod, REG_PWR_MGMT_1, 0x01);
     HAL_Delay(5);

     mpu_write(mod, REG_PWR_MGMT_2, 0x07);   // gyro standby
     mpu_write(mod, REG_CONFIG,      0x01);  // DLPF=1 (1kHz)

     mpu_write(mod, REG_SMPLRT_DIV, (1000 / 250) - 1);

     mpu_write(mod, REG_ACCEL_CONFIG, 0x00); // +-2g
 }

void mpu_sleep(MPU6050 *mod) {
	mpu_write(mod, REG_PWR_MGMT_1, 0x40);
}

HAL_StatusTypeDef mpu_read_accel(MPU6050 *mod, int16_t *ax, int16_t *ay, int16_t *az) {
    uint8_t buf[6] = {0};
    HAL_StatusTypeDef s = mpu_read(mod, REG_ACCEL_XOUT, buf, 6);
    if (s != HAL_OK) return s;
    *ax = (int16_t)(buf[0] << 8 | buf[1]);
    *ay = (int16_t)(buf[2] << 8 | buf[3]);
    *az = (int16_t)(buf[4] << 8 | buf[5]);
    return HAL_OK;
}
