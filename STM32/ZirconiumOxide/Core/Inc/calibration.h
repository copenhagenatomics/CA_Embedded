/*
 * calibration.h
 *
 *  Created on: Oct 18, 2021
 *      Author: matias
 */

#ifndef INC_CALIBRATION_H_
#define INC_CALIBRATION_H_

#include <stdbool.h>
#include <CAProtocol.h>
#include <oxygen.h>
#include "ZrO2Sensor.h"

#define NO_SENSOR_CHANNELS    2

#define DEFAULT_SENSORV_GAIN_LOW 13
#define DEFAULT_SENSORV_GAIN_HIGH 13
#define DEFAULT_GAIN 20

extern uint32_t _FlashAddrCal;   // Variable defined in ld linker script.
#define FLASH_ADDR_CAL ((uintptr_t) &_FlashAddrCal)

typedef struct CalibrationInfo
{
    bool calibrate;	// Indication of whether a calibration command has been sent
    double target;	// The target output level the sensor should be calibrated to
    bool calibrationErr;	// Calibration end flag if max/min wiper position has been reached
} CalibrationInfo;

int initiateCalibration(int channel);
void calibrateSensor(int noOfCalibrations, const CACalibration* calibrations);
void calibrateToTarget(ZrO2Device * dev, int channel, double oxygenLevel);
void initCalibration(ZrO2Device * dev1, ZrO2Device * dev2, CRC_HandleTypeDef* hcrc);
void calibratePort(CalibrationInfo *calibration, double currentLevel, int channel);
void calibrateManual(ZrO2Device * dev, int channel, int gain, float bias, float scalar, float quadrant);
void calibrationReadWrite(bool write);
void updateSlopeFlash(ZrO2Device * dev, int channel);
void updateCalibrationInfo();


#endif /* INC_CALIBRATION_H_ */
