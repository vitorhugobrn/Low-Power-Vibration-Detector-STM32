#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include <stdint.h>

/*
 * Magic value to verify if Flash contains valid config
 */
#define CONFIG_MAGIC 0xDEADBEEF

/*
 * STM32F446RETx Flash Memory Map (512 KB total):
 * Sector 0: 0x08000000 (16 KB)
 * Sector 1: 0x08004000 (16 KB)
 * Sector 2: 0x08008000 (16 KB)
 * Sector 3: 0x0800C000 (16 KB)
 * Sector 4: 0x08010000 (64 KB)
 * Sector 5: 0x08020000 (128 KB)
 * Sector 6: 0x08040000 (128 KB)
 * Sector 7: 0x08060000 (128 KB) <-- SAFE SECTOR FOR CONFIG DATA
 */

#define CONFIG_FLASH_SECTOR   FLASH_SECTOR_7
#define CONFIG_FLASH_ADDR     0x08060000

typedef enum {
	MODE_ISO_DEFAULT = 0, // 1.5x Baseline
	MODE_TOLERANT,	      // 2.0x Baseline
	MODE_HEAVY_DUTY,	  // 2.5x Baseline
	MODE_COUNT
} AlarmMode;

typedef struct {
    uint32_t magic;
    float rms_baseline;
    AlarmMode current_mode;

    float rms_alarm_thresh;
} SystemConfig;

typedef enum {
	ALARM_NORMAL = 0,
	ALARM_TRIGGERED
} AlarmState;

void Config_Load(SystemConfig *cfg);
void Config_Save(SystemConfig *cfg);
void Config_Set_Defaults(SystemConfig *cfg);
void Config_Apply_Mode(SystemConfig *cfg);

void DWT_Init(void);

#endif /* INC_CONFIG_H_ */
