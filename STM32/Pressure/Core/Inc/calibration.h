/*!
 * @file    calibration.h
 * @brief   Header file of calibration.c
 * @date    05/12/2022
 * @author  Matias
 */

#ifndef INC_CALIBRATION_H_
#define INC_CALIBRATION_H_

#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "main.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Default pressure sensors
#define GANLITONG_OFFSET -1.79    // in bar
#define GANLITONG_SCALAR 0.00188  // in bar/ADCstep assuming the ADC range is [0V, 5V]

#define PORTCALVAL_DEFAULT 1.0

#define NO_CALIBRATION_CHANNELS 6      // 6 Sensors
#define NO_CHANNELS             8      // 6 sensors + VCC + Vbus
#define ADC_MAX                 4095   // 12-bits
#define MAX_VIN                 5.112  // Maximum input voltage on channels (after voltage divider)
#define MAX_VCC_IN              5.49   // Maximum measurable VCC (after voltage divider)
#define V_REF                   3.3    // ADC internal voltage reference
#define VOLTAGE_SCALING         MAX_VIN / 5.0  // Necessary because LoopControl assumes [0V, 5V]

// Variables that need to be stored in flash memory.
typedef struct FlashCalibration {
    float sensorCalVal[NO_CHANNELS * 2];
    float portCalVal[NO_CHANNELS];
    int measurementType[NO_CHANNELS];
} FlashCalibration;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void calibrateBoard(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                    float *ADCMeansRaw, uint32_t calSize);
void calibrateSensor(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                     uint32_t calSize);
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size);
void calibrationRW(bool write, FlashCalibration *cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
