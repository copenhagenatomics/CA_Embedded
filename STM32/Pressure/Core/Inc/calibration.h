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
#include "vl53l1_api.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define PORTCALVAL_DEFAULT 1.0

#define NO_CHANNELS             2      // VCC + Vbus
#define ADC_MAX                 4095   // 12-bits
#define MAX_VIN                 5.112  // Maximum input voltage on channels (after voltage divider)
#define MAX_VCC_IN              5.49   // Maximum measurable VCC (after voltage divider)
#define V_REF                   3.3    // ADC internal voltage reference
#define VOLTAGE_SCALING         MAX_VIN / 5.0  // Necessary because LoopControl assumes [0V, 5V]

// Variables that need to be stored in flash memory.
typedef struct FlashCalibration {
    VL53L1_CustomerNvmManaged_t cal_data;
} FlashCalibration;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void calibrateBoard(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                    float *ADCMeansRaw, uint32_t calSize);
void calibrateSensor(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                     uint32_t calSize, VL53L1_DEV dev_p);
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size, VL53L1_DEV dev_p);
void calibrationRW(bool write, FlashCalibration *cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
