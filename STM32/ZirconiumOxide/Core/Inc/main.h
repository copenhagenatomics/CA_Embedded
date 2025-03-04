/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2022 STMicroelectronics.
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
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define Vref_Pin GPIO_PIN_0
#define Vref_GPIO_Port GPIOA
#define sense_out1_Pin GPIO_PIN_1
#define sense_out1_GPIO_Port GPIOA
#define sense_out2_Pin GPIO_PIN_3
#define sense_out2_GPIO_Port GPIOA
#define Current_sense2_Pin GPIO_PIN_5
#define Current_sense2_GPIO_Port GPIOA
#define Current_sense1_Pin GPIO_PIN_7
#define Current_sense1_GPIO_Port GPIOA
#define Voltage_sense2_Pin GPIO_PIN_0
#define Voltage_sense2_GPIO_Port GPIOB
#define Voltage_sense1_Pin GPIO_PIN_1
#define Voltage_sense1_GPIO_Port GPIOB
#define CS2_3_Pin GPIO_PIN_12
#define CS2_3_GPIO_Port GPIOB
#define CS2_1_Pin GPIO_PIN_13
#define CS2_1_GPIO_Port GPIOB
#define CS2_2_Pin GPIO_PIN_14
#define CS2_2_GPIO_Port GPIOB
#define CS1_3_Pin GPIO_PIN_15
#define CS1_3_GPIO_Port GPIOB
#define CS1_1_Pin GPIO_PIN_8
#define CS1_1_GPIO_Port GPIOA
#define CS1_2_Pin GPIO_PIN_9
#define CS1_2_GPIO_Port GPIOA
#define heater_ctrl_2_Pin GPIO_PIN_4
#define heater_ctrl_2_GPIO_Port GPIOB
#define heater_ctrl_1_Pin GPIO_PIN_5
#define heater_ctrl_1_GPIO_Port GPIOB
#define UD_Pin GPIO_PIN_6
#define UD_GPIO_Port GPIOB
#define CLK_Pin GPIO_PIN_7
#define CLK_GPIO_Port GPIOB
#define sense_ctrl_2_Pin GPIO_PIN_8
#define sense_ctrl_2_GPIO_Port GPIOB
#define sense_ctrl_1_Pin GPIO_PIN_9
#define sense_ctrl_1_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
