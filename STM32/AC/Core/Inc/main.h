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
#define fanctrl_Pin GPIO_PIN_14
#define fanctrl_GPIO_Port GPIOC
#define hall1_Pin GPIO_PIN_0
#define hall1_GPIO_Port GPIOA
#define hall2_Pin GPIO_PIN_1
#define hall2_GPIO_Port GPIOA
#define hall3_Pin GPIO_PIN_2
#define hall3_GPIO_Port GPIOA
#define hall4_Pin GPIO_PIN_3
#define hall4_GPIO_Port GPIOA
#define temp1_Pin GPIO_PIN_5
#define temp1_GPIO_Port GPIOA
#define temp2_Pin GPIO_PIN_6
#define temp2_GPIO_Port GPIOA
#define temp3_Pin GPIO_PIN_7
#define temp3_GPIO_Port GPIOA
#define temp4_Pin GPIO_PIN_0
#define temp4_GPIO_Port GPIOB
#define powerStatus_Pin GPIO_PIN_2
#define powerStatus_GPIO_Port GPIOB
#define ctrl1_Pin GPIO_PIN_6
#define ctrl1_GPIO_Port GPIOB
#define ctrl2_Pin GPIO_PIN_7
#define ctrl2_GPIO_Port GPIOB
#define ctrl3_Pin GPIO_PIN_8
#define ctrl3_GPIO_Port GPIOB
#define ctrl4_Pin GPIO_PIN_9
#define ctrl4_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
