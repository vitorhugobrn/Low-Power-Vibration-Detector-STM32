/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "acquisition.h"
#include "config.h"

#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
SystemConfig sys_cfg = { 0 };
MPU6050 mpu = {
	.hi2c = &hi2c1,
	.addr = MPU_ADDR
};

static volatile uint8_t wake_by_rtc		 = 0;
static volatile uint8_t wake_by_cal_btn  = 0;
static volatile uint8_t wake_by_mode_btn = 0;

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc) {
	wake_by_rtc = 1;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == CAL_BTN_Pin)  wake_by_cal_btn  = 1;
	if (GPIO_Pin == MODE_BTN_Pin) wake_by_mode_btn = 1;
}

void evaluate_alarm(VibrationReading vibration) {

	if (vibration.rms_total > sys_cfg.rms_alarm_thresh) {
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_Delay(0xFFFFFFFF);
	}
}

void task_recalibrate() {
	VibrationReading vibration = acquisition_task(&mpu);
	sys_cfg.rms_baseline = vibration.rms_total;
	Config_Apply_Mode(&sys_cfg);
	Config_Save(&sys_cfg);

	for (int i = 0; i < 4; i++) {
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
		HAL_Delay(100);
	}
}

void handle_mode_change(void) {
    sys_cfg.current_mode = (sys_cfg.current_mode + 1) % MODE_COUNT;
    Config_Apply_Mode(&sys_cfg);
    Config_Save(&sys_cfg);

    // 1 blink  = Default    (1.5x)
    // 2 blinks = Tolerant   (2.0x)
    // 3 blinks = Heavy Duty (2.5x)
    HAL_Delay(300);
    for (int i = 0; i <= sys_cfg.current_mode; i++) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
        HAL_Delay(250);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
        HAL_Delay(350);
    }
}

void config_mode_task(void) {
    for (int i = 0; i < 4; i++) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        HAL_Delay(150);
    }

    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET) {
        if (wake_by_cal_btn) {
            wake_by_cal_btn = 0;
            task_recalibrate();
        }

        if (wake_by_mode_btn) {
            wake_by_mode_btn = 0;
            handle_mode_change();
        }

        HAL_Delay(50);
    }

    for (int i = 0; i < 2; i++) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        HAL_Delay(300);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    uint8_t woke_from_standby = __HAL_PWR_GET_FLAG(PWR_FLAG_SB);
    uint8_t woke_from_pin     = __HAL_PWR_GET_FLAG(PWR_FLAG_WU);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_SB);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_RTC_Init();

    Config_Load(&sys_cfg);
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    woke_from_pin = woke_from_pin && (HAL_GPIO_ReadPin(TOGGLE_WAKE_GPIO_Port, TOGGLE_WAKE_Pin) == GPIO_PIN_SET);
    uint8_t woke_from_rtc = woke_from_standby && __HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTF);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);

    if (woke_from_standby) {
        if (woke_from_rtc && !woke_from_pin) {
            VibrationReading vibration = acquisition_task(&mpu);
            evaluate_alarm(vibration);
        } else if (woke_from_pin) {
            config_mode_task();
        }
    }

    HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
    __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
    __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();

    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 10, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK) {
        Error_Handler();
    }

    mpu_sleep(&mpu);
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_SuspendTick();
    HAL_PWR_EnterSTANDBYMode();
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */

static void MX_RTC_Init(void)
{
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

    /* USER CODE BEGIN RTC_Init 0 */
    // Guard must be before HAL_RTC_Init — backup domain survives Standby
    // but HAL_RTC_Init can reset it if clock source changes
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xBEEF) {
        // Re-init handle only, don't touch time/date/wakeup timer
        if (HAL_RTC_Init(&hrtc) != HAL_OK) {
            Error_Handler();
        }
        return;
    }
    /* USER CODE END RTC_Init 0 */

    if (HAL_RTC_Init(&hrtc) != HAL_OK) {
        Error_Handler();
    }

    // Cold boot only from here
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    sTime.Hours   = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;
    sTime.DayLightSaving  = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation  = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK) {
        Error_Handler();
    }

    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month   = RTC_MONTH_JANUARY;
    sDate.Date    = 0x1;
    sDate.Year    = 0x0;
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK) {
        Error_Handler();
    }

    // Don't arm wakeup timer here — main() does it with correct values
    // HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, ...) ← remove this entirely

    /* USER CODE BEGIN RTC_Init 2 */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0xBEEF);
    /* USER CODE END RTC_Init 2 */
}
//static void MX_RTC_Init(void)
//{
//
//  /* USER CODE BEGIN RTC_Init 0 */
//
//  /* USER CODE END RTC_Init 0 */
//
//  RTC_TimeTypeDef sTime = {0};
//  RTC_DateTypeDef sDate = {0};
//
//  /* USER CODE BEGIN RTC_Init 1 */
//
//  /* USER CODE END RTC_Init 1 */
//
//  /** Initialize RTC Only
//  */
//  hrtc.Instance = RTC;
//  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
//  hrtc.Init.AsynchPrediv = 127;
//  hrtc.Init.SynchPrediv = 255;
//  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
//  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
//  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
//  if (HAL_RTC_Init(&hrtc) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /* USER CODE BEGIN Check_RTC_BKUP */
//  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == 0xBEEF) {
//      // Wakeup from Standby — RTC already configured, skip everything
//      return;
//  }
//  /* USER CODE END Check_RTC_BKUP */
//
//  /** Initialize RTC and set the Time and Date
//  */
//  sTime.Hours = 0x0;
//  sTime.Minutes = 0x0;
//  sTime.Seconds = 0x0;
//  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
//  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
//  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
//  sDate.Month = RTC_MONTH_JANUARY;
//  sDate.Date = 0x1;
//  sDate.Year = 0x0;
//
//  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
//  {
//    Error_Handler();
//  }
//
//  /** Enable the WakeUp
//  */
//  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
//  {
//    Error_Handler();
//  }
//  /* USER CODE BEGIN RTC_Init 2 */
//  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0xBEEF);
//  /* USER CODE END RTC_Init 2 */
//
//}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : TOGGLE_WAKE_Pin */
  GPIO_InitStruct.Pin = TOGGLE_WAKE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(TOGGLE_WAKE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MODE_BTN_Pin CAL_BTN_Pin */
  GPIO_InitStruct.Pin = MODE_BTN_Pin|CAL_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
