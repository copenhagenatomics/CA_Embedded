/*!
 * @file    calibration.h
 * @brief   Header file of calibration.c
 * @date	22/05/2025
 * @author 	Timothé Dodin
 */

#ifndef INC_CALIBRATION_H_
#define INC_CALIBRATION_H_

#include <stdbool.h>
#include <stdint.h>
#include "CAProtocol.h"
#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define NO_OF_SENSORS 6

typedef struct sensorCalibration {
    float resP1;    // kOhm     - Resistance in series with P1 terminal
    float resP2;    // kOhm     - Resistance in series with P2 terminal
    float resN1;    // kOhm     - Resistance in serie with N1 terminal
    float vScalar;  // Resistive divider ratio
} sensorCalibration_t;

typedef struct FlashCalibration {
    sensorCalibration_t sensorCal[NO_OF_SENSORS];
    float boostScalar;  // Resistive divider ratio
} FlashCalibration_t;

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void calibration(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration_t *cal,
                 uint32_t size, float sensorVoltages[NO_OF_SENSORS], float voltageBoost);
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration_t *cal, uint32_t size);
void calibrationRW(bool write, FlashCalibration_t *cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
