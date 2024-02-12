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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_15
#define LED_GPIO_Port GPIOC
#define CTRL_3_Pin GPIO_PIN_0
#define CTRL_3_GPIO_Port GPIOA
#define CTRL_4_Pin GPIO_PIN_1
#define CTRL_4_GPIO_Port GPIOA
#define CTRL_6_Pin GPIO_PIN_2
#define CTRL_6_GPIO_Port GPIOA
#define CTRL_5_Pin GPIO_PIN_3
#define CTRL_5_GPIO_Port GPIOA
#define Hall_1_Pin GPIO_PIN_4
#define Hall_1_GPIO_Port GPIOA
#define Hall_2_Pin GPIO_PIN_5
#define Hall_2_GPIO_Port GPIOA
#define Hall_3_Pin GPIO_PIN_6
#define Hall_3_GPIO_Port GPIOA
#define Hall_4_Pin GPIO_PIN_7
#define Hall_4_GPIO_Port GPIOA
#define Hall_5_Pin GPIO_PIN_0
#define Hall_5_GPIO_Port GPIOB
#define Hall_6_Pin GPIO_PIN_1
#define Hall_6_GPIO_Port GPIOB
#define SENSE_24V_Pin GPIO_PIN_8
#define SENSE_24V_GPIO_Port GPIOA
#define Btn_1_Pin GPIO_PIN_15
#define Btn_1_GPIO_Port GPIOA
#define Btn_2_Pin GPIO_PIN_3
#define Btn_2_GPIO_Port GPIOB
#define Btn_3_Pin GPIO_PIN_4
#define Btn_3_GPIO_Port GPIOB
#define Btn_4_Pin GPIO_PIN_5
#define Btn_4_GPIO_Port GPIOB
#define CTRL_2_Pin GPIO_PIN_6
#define CTRL_2_GPIO_Port GPIOB
#define CTRL_1_Pin GPIO_PIN_7
#define CTRL_1_GPIO_Port GPIOB
#define Btn_5_Pin GPIO_PIN_8
#define Btn_5_GPIO_Port GPIOB
#define Btn_6_Pin GPIO_PIN_9
#define Btn_6_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
