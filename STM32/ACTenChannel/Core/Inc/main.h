/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "stm32f4xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FAN_CTRL_Pin GPIO_PIN_14
#define FAN_CTRL_GPIO_Port GPIOC
#define POWERSTATUS_Pin GPIO_PIN_2
#define POWERSTATUS_GPIO_Port GPIOB
#define CTRL_5_Pin GPIO_PIN_12
#define CTRL_5_GPIO_Port GPIOB
#define CTRL_6_Pin GPIO_PIN_13
#define CTRL_6_GPIO_Port GPIOB
#define CTRL_7_Pin GPIO_PIN_14
#define CTRL_7_GPIO_Port GPIOB
#define CTRL_8_Pin GPIO_PIN_15
#define CTRL_8_GPIO_Port GPIOB
#define CTRL_9_Pin GPIO_PIN_8
#define CTRL_9_GPIO_Port GPIOA
#define DQ1_Pin GPIO_PIN_9
#define DQ1_GPIO_Port GPIOA
#define CTRL_LED_Pin GPIO_PIN_3
#define CTRL_LED_GPIO_Port GPIOB
#define CTRL_4_Pin GPIO_PIN_5
#define CTRL_4_GPIO_Port GPIOB
#define CTRL_3_Pin GPIO_PIN_6
#define CTRL_3_GPIO_Port GPIOB
#define CTRL_2_Pin GPIO_PIN_7
#define CTRL_2_GPIO_Port GPIOB
#define CTRL_1_Pin GPIO_PIN_8
#define CTRL_1_GPIO_Port GPIOB
#define CTRL_0_Pin GPIO_PIN_9
#define CTRL_0_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
