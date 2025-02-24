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
#define ADC10_Pin GPIO_PIN_0
#define ADC10_GPIO_Port GPIOA
#define ADC9_Pin GPIO_PIN_1
#define ADC9_GPIO_Port GPIOA
#define ADC8_Pin GPIO_PIN_2
#define ADC8_GPIO_Port GPIOA
#define ADC7_Pin GPIO_PIN_3
#define ADC7_GPIO_Port GPIOA
#define ADC6_Pin GPIO_PIN_4
#define ADC6_GPIO_Port GPIOA
#define ADC5_Pin GPIO_PIN_5
#define ADC5_GPIO_Port GPIOA
#define ADC4_Pin GPIO_PIN_6
#define ADC4_GPIO_Port GPIOA
#define ADC3_Pin GPIO_PIN_7
#define ADC3_GPIO_Port GPIOA
#define ADC2_Pin GPIO_PIN_0
#define ADC2_GPIO_Port GPIOB
#define ADC1_Pin GPIO_PIN_1
#define ADC1_GPIO_Port GPIOB
#define Vout_ref_Pin GPIO_PIN_12
#define Vout_ref_GPIO_Port GPIOB
#define Power_EN_Boost_Pin GPIO_PIN_10
#define Power_EN_Boost_GPIO_Port GPIOA
#define LED_Pin GPIO_PIN_3
#define LED_GPIO_Port GPIOB
#define test1_Pin GPIO_PIN_4
#define test1_GPIO_Port GPIOB
#define test2_Pin GPIO_PIN_5
#define test2_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
