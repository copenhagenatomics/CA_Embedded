/*
 * calibration.h
 *
 *  Created on: 5 Dec 2022
 *      Author: matias
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
#define GANLITONG_OFFSET  	-1.79
#define GANLITONG_SCALAR  	 0.00188

#define PORTCALVAL_DEFAULT  0.99

#define NO_CALIBRATION_CHANNELS		6
#define NO_CHANNELS                 8
#define ADC_MAX				        4095
#define MAX_VIN				        5 // Maximum input voltage on channels
#define MAX_VCC_IN			        5.49

// Variables that need to be stored in flash memory.
typedef struct FlashCalibration
{
    float sensorCalVal[NO_CHANNELS*2];
    float portCalVal[NO_CHANNELS];
    int	measurementType[NO_CHANNELS];
} FlashCalibration;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void calibrateBoard(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration *cal, float *ADCMeansRaw, uint32_t calSize);
void calibrateSensor(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration *cal, uint32_t calSize);
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size);
void calibrationRW(bool write, FlashCalibration *cal, uint32_t size);
void setSensorCalibration(FlashCalibration *cal, int i, char type);

#endif /* INC_CALIBRATION_H_ */
