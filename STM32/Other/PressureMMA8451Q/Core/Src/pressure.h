/*!
 * @file    pressure.h
 * @brief   Header file of pressure.c
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#ifndef INC_PRESSURE_H_
#define INC_PRESSURE_H_

#include <stdbool.h>

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Pressure board status register definitions */

/* Set when the MMA8451Q accelerometer is not responding over I2C */
#define ACCEL_ERROR_Pos 0U
#define ACCEL_ERROR_Msk (1U << ACCEL_ERROR_Pos)

/* Define showing which bits are "errors" and which are only for information */
#define PRESSURE_ERROR_Msk (BS_SYSTEM_ERRORS_Msk | ACCEL_ERROR_Msk)

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void pressureInit(ADC_HandleTypeDef *hadc, I2C_HandleTypeDef *hi2c);
void pressureLoop(const char *bootMsg);

#endif /* INC_PRESSURE_H_ */
