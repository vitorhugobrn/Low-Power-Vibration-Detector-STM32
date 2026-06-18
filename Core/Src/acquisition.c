#include "acquisition.h"

#include <math.h>

static int16_t ax_buf[N_SAMPLES];
static int16_t ay_buf[N_SAMPLES];
static int16_t az_buf[N_SAMPLES];

static VibrationReading compute_metrics(void) {
    VibrationReading r = { 0 };

    int32_t sum_x = 0, sum_y = 0, sum_z = 0;
    for (int i = 0; i < N_SAMPLES; i++) {
        sum_x += ax_buf[i]; sum_y += ay_buf[i]; sum_z += az_buf[i];
    }
    int16_t mean_x = sum_x / N_SAMPLES;
    int16_t mean_y = sum_y / N_SAMPLES;
    int16_t mean_z = sum_z / N_SAMPLES;

    uint64_t sq_x = 0, sq_y = 0, sq_z = 0;

    for (int i = 0; i < N_SAMPLES; i++) {
        int16_t dx = ax_buf[i] - mean_x;
        int16_t dy = ay_buf[i] - mean_y;
        int16_t dz = az_buf[i] - mean_z;

        sq_x += (int32_t)dx * dx;
        sq_y += (int32_t)dy * dy;
        sq_z += (int32_t)dz * dz;
    }

    const float LSB_PER_G = 16384.0f; // ±2g
    float rms_x = sqrtf((float)sq_x / N_SAMPLES) / LSB_PER_G;
    float rms_y = sqrtf((float)sq_y / N_SAMPLES) / LSB_PER_G;
    float rms_z = sqrtf((float)sq_z / N_SAMPLES) / LSB_PER_G;

    r.rms_total = sqrtf(rms_x*rms_x + rms_y*rms_y + rms_z*rms_z);
    r.timestamp = HAL_GetTick();
    return r;
}

void DWT_Init(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

VibrationReading acquisition_task(MPU6050 *mod) {
    mpu_wake(mod);
    HAL_Delay(50);

    const uint32_t cycles_per_sample = SystemCoreClock / SAMPLE_RATE_HZ;

    for (int i = 0; i < N_SAMPLES; i++) {
        uint32_t t0 = DWT->CYCCNT;

        uint8_t retries = 3;
        while (mpu_read_accel(mod, &ax_buf[i], &ay_buf[i], &az_buf[i]) != HAL_OK) {
            if (--retries == 0) {
                ax_buf[i] = (i > 0) ? ax_buf[i-1] : 0;
                ay_buf[i] = (i > 0) ? ay_buf[i-1] : 0;
                az_buf[i] = (i > 0) ? az_buf[i-1] : 0;
                break;
            }
            HAL_Delay(1);
        }

        while ((DWT->CYCCNT - t0) < cycles_per_sample);
    }

    mpu_sleep(mod);
    return compute_metrics();
}