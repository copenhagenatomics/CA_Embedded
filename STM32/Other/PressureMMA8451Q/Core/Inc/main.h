/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
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
#define CH6_Ctrl_Pin GPIO_PIN_13
#define CH6_Ctrl_GPIO_Port GPIOC
#define CH5_Ctrl_Pin GPIO_PIN_14
#define CH5_Ctrl_GPIO_Port GPIOC
#define CH4_Ctrl_Pin GPIO_PIN_15
#define CH4_Ctrl_GPIO_Port GPIOC
#define FB_5V_Pin GPIO_PIN_0
#define FB_5V_GPIO_Port GPIOA
#define CH3_Ctrl_Pin GPIO_PIN_12
#define CH3_Ctrl_GPIO_Port GPIOB
#define CH2_Ctrl_Pin GPIO_PIN_13
#define CH2_Ctrl_GPIO_Port GPIOB
#define CH1_Ctrl_Pin GPIO_PIN_14
#define CH1_Ctrl_GPIO_Port GPIOB
#define CTRL_LED_Pin GPIO_PIN_4
#define CTRL_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
