/*!
 * @file    pressure.h
 * @brief   Header file of pressure.c
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#ifndef INC_PRESSURE_H_
#define INC_PRESSURE_H_

#include <stdbool.h>

#include "calibration.h"
#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Pressure board status register definitions */

/* Ranging error */
#define RANGING_ERROR_Pos 10U
#define RANGING_ERROR_Msk (1U << RANGING_ERROR_Pos)

/* VCC / VCC Raw monitoring bits. These are set when the input voltage is off. */
#define VCC_RAW_ERROR_Pos 9U
#define VCC_RAW_ERROR_Msk (1U << VCC_RAW_ERROR_Pos)

#define VCC_ERROR_Pos 8U
#define VCC_ERROR_Msk (1U << VCC_ERROR_Pos)

#define VL53_STATUS_Pos 0U
#define VL53_STATUS_Msk ((1U << 8U) - 1U)

/* Define showing which bits are "errors" and which are only for information */
#define PRESSURE_ERROR_Msk (BS_SYSTEM_ERRORS_Msk | VCC_ERROR_Msk | VCC_RAW_ERROR_Msk)

/* Common definitions for Pressure Board */
#define MIN_VCC     5.0
#define MIN_RAW_VCC 4.6

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc, I2C_HandleTypeDef *hi2c);
void pressureLoop(const char *bootMsg);

#endif /* INC_PRESSURE_H_ */
