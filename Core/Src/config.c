#include "config.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <string.h>

static const float mode_multipliers[MODE_COUNT] = {
	[MODE_ISO_DEFAULT] = 1.5f,
	[MODE_TOLERANT]    = 2.0f,
	[MODE_HEAVY_DUTY]  = 2.5f
};

void Config_Set_Defaults(SystemConfig *cfg) {
    cfg->magic = CONFIG_MAGIC;
    cfg->rms_baseline = 0.0f;
    cfg->current_mode = MODE_ISO_DEFAULT;
    Config_Apply_Mode(cfg);
}

void Config_Apply_Mode(SystemConfig *cfg) {
	if (cfg->current_mode >= MODE_COUNT)
		cfg->current_mode = MODE_ISO_DEFAULT;

	cfg->rms_alarm_thresh = cfg->rms_baseline * mode_multipliers[cfg->current_mode];
}

void Config_Load(SystemConfig *cfg) {
    memcpy(cfg, (void*)CONFIG_FLASH_ADDR, sizeof(SystemConfig));

    if (cfg->magic != CONFIG_MAGIC) {
        Config_Set_Defaults(cfg);
    }

    Config_Apply_Mode(cfg);
}

void Config_Save(SystemConfig *cfg) {
    cfg->magic = CONFIG_MAGIC;

    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector        = CONFIG_FLASH_SECTOR;
    EraseInitStruct.NbSectors     = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        HAL_FLASH_Lock();
        return;
    }

    uint32_t *src = (uint32_t*)cfg;
    uint32_t words_to_write = (sizeof(SystemConfig) + 3) / 4;

    for (uint32_t i = 0; i < words_to_write; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                              CONFIG_FLASH_ADDR + (i * 4),
                              src[i]) != HAL_OK) {
            break;
        }
    }

    HAL_FLASH_Lock();
}
