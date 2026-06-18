/*
 * acquisition.h
 *
 *  Created on: May 26, 2026
 *      Author: Aluno
 */

#ifndef INC_ACQUISITION_H_
#define INC_ACQUISITION_H_

#include <stdint.h>
#include "mpu6050.h"

#define SAMPLE_RATE_HZ    250
#define SAMPLE_DURATION_S 2
#define N_SAMPLES         (SAMPLE_RATE_HZ * SAMPLE_DURATION_S) // 500

typedef struct {
    float rms_total;
    uint32_t timestamp;
} VibrationReading;

VibrationReading acquisition_task(MPU6050 *mod);

#endif /* INC_ACQUISITION_H_ */
