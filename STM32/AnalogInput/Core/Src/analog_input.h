/*!
 * @file    analog_input.h
 * @brief   Header file of analog_input.c
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#ifndef INC_ANALOG_INPUT_H_
#define INC_ANALOG_INPUT_H_

#include <stdbool.h>

#include "calibration.h"
#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* AnalogInput board status register definitions */

/* VCC / VCC Raw monitoring bits. These are set when the input voltage is off. */
#define VCC_RAW_ERROR_Pos 8U
#define VCC_RAW_ERROR_Msk (1U << VCC_RAW_ERROR_Pos)

#define VCC_ERROR_Pos 7U
#define VCC_ERROR_Msk (1U << VCC_ERROR_Pos)

/* Status bits showing whether a port is in voltage or current measurement mode.
   0 = Voltage, 1 = Current  */
#define PORT_MEASUREMENT_TYPE(x) (1U << (x))

/* Define showing which bits are "errors" and which are only for information */
#define ANALOG_INPUT_ERROR_Msk (BS_SYSTEM_ERRORS_Msk | VCC_ERROR_Msk | VCC_RAW_ERROR_Msk)

/* Common definitions for AnalogInput Board */
#define MIN_VCC     5.0
#define MIN_RAW_VCC 4.6

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void analogInputInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc);
void analogInputLoop(const char *bootMsg);

#endif /* INC_ANALOG_INPUT_H_ */
