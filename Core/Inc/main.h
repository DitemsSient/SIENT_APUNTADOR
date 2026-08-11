/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define INTERRUPCION_IMU_Pin GPIO_PIN_0
#define INTERRUPCION_IMU_GPIO_Port GPIOA
#define CS_FLASH_Pin GPIO_PIN_1
#define CS_FLASH_GPIO_Port GPIOA
#define Tx_Laser_Pin GPIO_PIN_2
#define Tx_Laser_GPIO_Port GPIOA
#define MUX_SEL_C_Pin GPIO_PIN_3
#define MUX_SEL_C_GPIO_Port GPIOA
#define MUX_SEL_CA4_Pin GPIO_PIN_4
#define MUX_SEL_CA4_GPIO_Port GPIOA
#define USB_ID_Pin GPIO_PIN_5
#define USB_ID_GPIO_Port GPIOA
#define BLUETOOTH_MCU_Pin GPIO_PIN_6
#define BLUETOOTH_MCU_GPIO_Port GPIOA
#define MUX_SEL_CA7_Pin GPIO_PIN_7
#define MUX_SEL_CA7_GPIO_Port GPIOA
#define RGB_R_Pin GPIO_PIN_0
#define RGB_R_GPIO_Port GPIOB
#define RGB_G_Pin GPIO_PIN_1
#define RGB_G_GPIO_Port GPIOB
#define RGB_B_Pin GPIO_PIN_2
#define RGB_B_GPIO_Port GPIOB
#define BOTON_B_Pin GPIO_PIN_10
#define BOTON_B_GPIO_Port GPIOB
#define BOTON_A_Pin GPIO_PIN_11
#define BOTON_A_GPIO_Port GPIOB
#define SELECTOR_MCU_Pin GPIO_PIN_12
#define SELECTOR_MCU_GPIO_Port GPIOB
#define GATILLO_Pin GPIO_PIN_8
#define GATILLO_GPIO_Port GPIOA
#define MCU_TX_Pin GPIO_PIN_9
#define MCU_TX_GPIO_Port GPIOA
#define MCU_RX_Pin GPIO_PIN_10
#define MCU_RX_GPIO_Port GPIOA
#define BUZZER_Pin GPIO_PIN_3
#define BUZZER_GPIO_Port GPIOB
#define BLUETOOTH_VSP_Pin GPIO_PIN_8
#define BLUETOOTH_VSP_GPIO_Port GPIOB
#define BLUETOOTH_AUTORUN_Pin GPIO_PIN_9
#define BLUETOOTH_AUTORUN_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
