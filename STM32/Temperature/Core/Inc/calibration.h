/*!
 * @file    calibration.h
 * @brief   Header file of calibration.c
 * @date    25/11/2026
 * @author  Timothé Dodin
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

#define NO_SPI_DEVICES 5                     // Number of external ADCs
#define NO_PORTS       (2 * NO_SPI_DEVICES)  // Number of temperature ports

typedef struct FlashCalibration {
    struct {
        int type;       // Temperature sensor type (sensorType_e)
        float leadRes;  // Resistance of the leads (for RTD sensors)
    } port[NO_PORTS];
} FlashCalibration_t;

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void calibrate(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration_t* cal,
               bool rtd);
void calibrationInit(CRC_HandleTypeDef* hcrc, FlashCalibration_t* cal, uint32_t size, bool rtd);
void printCal(const FlashCalibration_t* cal);
void readCalFromFlash();
void writeCalToFlash(const FlashCalibration_t* cal, uint32_t size);

#endif /* INC_CALIBRATION_H_ */
