/*!
 * @file    calibration.h
 * @brief   Header file of calibration.c
 * @date    23/11/2026
 * @author  Timothé Dodin
 */

#ifndef INC_CALIBRATION_H_
#define INC_CALIBRATION_H_

#include <stdint.h>

#include "CAProtocol.h"
#include "DCBoard.h"
#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

typedef struct _FlashCalibration {
    float currentLimits[DC_BOARD_NUM_PORTS];
} FlashCalibration_t;

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void calibrate(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration_t* cal);
void calibrationInit(CRC_HandleTypeDef* hcrc, FlashCalibration_t* cal, uint32_t size);
void printCal(FlashCalibration_t* cal);
void readCalFromFlash();
void writeCalToFlash(FlashCalibration_t* cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
