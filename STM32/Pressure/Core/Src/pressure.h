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

/* I2C errors */
#define I2C_ERROR_Pos 9U
#define I2C_ERROR_Msk(x) (1U << ((x) + I2C_ERROR_Pos))

/* VCC / VCC Raw monitoring bits. These are set when the input voltage is off. */
#define VCC_RAW_ERROR_Pos 8U
#define VCC_RAW_ERROR_Msk (1U << VCC_RAW_ERROR_Pos)

#define VCC_ERROR_Pos 7U
#define VCC_ERROR_Msk (1U << VCC_ERROR_Pos)

/* Status bits showing whether a port is in voltage or current measurement mode.
   0 = Voltage, 1 = Current  */
#define PORT_MEASUREMENT_TYPE(x) (1U << (x))

/* Define showing which bits are "errors" and which are only for information */
#define PRESSURE_ERROR_Msk (BS_SYSTEM_ERRORS_Msk | VCC_ERROR_Msk | VCC_RAW_ERROR_Msk | \
                            I2C_ERROR_Msk(0) | I2C_ERROR_Msk(1) | I2C_ERROR_Msk(2) | \
                            I2C_ERROR_Msk(3) | I2C_ERROR_Msk(4) | I2C_ERROR_Msk(5))

/* Common definitions for Pressure Board */
#define MIN_VCC     5.0
#define MIN_RAW_VCC 4.6

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc, I2C_HandleTypeDef *hi2c);
void pressureLoop(const char *bootMsg);

#endif /* INC_PRESSURE_H_ */
