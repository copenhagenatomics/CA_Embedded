#pragma once
#include <stdbool.h>

#include "stm32f4xx_hal.h"
#include "calibration.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Pressure board status register definitions */

/* VCC / VCC Raw monitoring bits. These are set when the input voltage is off. */
#define VCC_RAW_ERROR_Pos           8U
#define VCC_RAW_ERROR_Msk           (1U << VCC_RAW_ERROR_Pos)

#define VCC_ERROR_Pos               7U
#define VCC_ERROR_Msk               (1U << VCC_ERROR_Pos)

/* Status bits showing whether a port is in voltage or current measurement mode.
   0 = Voltage, 1 = Current  */
#define PORT_MEASUREMENT_TYPE(x)    (1U << (x))

/* Define showing which bits are "errors" and which are only for information */
#define PRESSURE_ERROR_Msk          (BS_SYSTEM_ERRORS_Msk | VCC_ERROR_Msk | VCC_RAW_ERROR_Msk)

/* Common definitions for Pressure Board */
#define MIN_VCC         5.0
#define MIN_RAW_VCC     4.6

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc);
void pressureLoop(const char* bootMsg);
