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

#define NO_CALIBRATION_CHANNELS 6        // 6 Sensors
#define ADC_MAX                 4095     // 12-bits
#define MAX_VBUS_IN             5.7750f  // Maximum measurable VCC (due to voltage divider)
#define MAX_28V_IN              33.0f    // Maximum measurable 28V rail (due to voltage divider)
#define V_REF                   3.3f     // ADC internal voltage reference
#define AMPS_TO_MILLIAMPS       1000.0f

enum adc_channels {
    ADC_CHANNEL_PORT_1 = 0,
    ADC_CHANNEL_PORT_2,
    ADC_CHANNEL_PORT_3,
    ADC_CHANNEL_PORT_4,
    ADC_CHANNEL_PORT_5,
    ADC_CHANNEL_PORT_6,
    ADC_CHANNEL_28V,
    ADC_CHANNEL_VBUS,
    NUM_CHANNELS,
};

// Variables that need to be stored in flash memory.
typedef struct FlashCalibration {
    float sensorCalVal[NUM_CHANNELS * 2];  // Slope and offset to convert volt/amp to user unit
    float portVoltCalVal[NUM_CHANNELS];    // Slope to convert ADC to volt
    float portResCalVal[NUM_CHANNELS];     // Shunt resistance to convert voltage to current
    int measurementType[NUM_CHANNELS];
} FlashCalibration;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void calibrateBoard(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                    float *ADCMeansRaw, uint32_t calSiz, float vref);
void calibrateSensor(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                     uint32_t calSize);
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size);
void calibrationRW(bool write, FlashCalibration *cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
