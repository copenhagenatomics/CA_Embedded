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
#define Test1_Pin GPIO_PIN_0
#define Test1_GPIO_Port GPIOB
#define Test2_Pin GPIO_PIN_1
#define Test2_GPIO_Port GPIOB
#define LED_Pin GPIO_PIN_2
#define LED_GPIO_Port GPIOB
#define CS1_Pin GPIO_PIN_13
#define CS1_GPIO_Port GPIOB
#define DRDY1_Pin GPIO_PIN_14
#define DRDY1_GPIO_Port GPIOB
#define CS2_Pin GPIO_PIN_15
#define CS2_GPIO_Port GPIOB
#define DRDY2_Pin GPIO_PIN_8
#define DRDY2_GPIO_Port GPIOA
#define CS3_Pin GPIO_PIN_15
#define CS3_GPIO_Port GPIOA
#define DRDY3_Pin GPIO_PIN_3
#define DRDY3_GPIO_Port GPIOB
#define CS4_Pin GPIO_PIN_4
#define CS4_GPIO_Port GPIOB
#define DRDY4_Pin GPIO_PIN_5
#define DRDY4_GPIO_Port GPIOB
#define CS5_Pin GPIO_PIN_8
#define CS5_GPIO_Port GPIOB
#define DRDY5_Pin GPIO_PIN_9
#define DRDY5_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
