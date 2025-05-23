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
#define SENSE_OUT1_Pin GPIO_PIN_0
#define SENSE_OUT1_GPIO_Port GPIOA
#define SENSE_OUT2_Pin GPIO_PIN_1
#define SENSE_OUT2_GPIO_Port GPIOA
#define SENSE_OUT3_Pin GPIO_PIN_2
#define SENSE_OUT3_GPIO_Port GPIOA
#define SENSE_OUT4_Pin GPIO_PIN_3
#define SENSE_OUT4_GPIO_Port GPIOA
#define SENSE_OUT5_Pin GPIO_PIN_4
#define SENSE_OUT5_GPIO_Port GPIOA
#define SENSE_OUT6_Pin GPIO_PIN_5
#define SENSE_OUT6_GPIO_Port GPIOA
#define VOUT_REF_Pin GPIO_PIN_6
#define VOUT_REF_GPIO_Port GPIOA
#define VCC_FB_Pin GPIO_PIN_7
#define VCC_FB_GPIO_Port GPIOA
#define BOOST_EN_Pin GPIO_PIN_10
#define BOOST_EN_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
