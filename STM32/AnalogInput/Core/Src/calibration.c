/*!
 * @file    calibration.h
 * @brief   Header file of calibration.c
 * @date    05/12/2022
 * @author  Matias
 */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "FLASH_readwrite.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "calibration.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t) & _FlashAddrCal)

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void setMeasurementType(int channel, int measureCurrent);
static void channelGpioInit(FlashCalibration *cal);
static void setDefaultCalibration(FlashCalibration *cal);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

CRC_HandleTypeDef *hcrc_ = NULL;
StmGpio CH_Ctrl[NO_CALIBRATION_CHANNELS];

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Define measurement type (voltage/current)
 * @param   channel Sensor channel (0-5)
 * @param   measureCurrent >0 if current measurement
 */
static void setMeasurementType(int channel, int measureCurrent) {
    stmSetGpio(CH_Ctrl[channel], measureCurrent);
}

/*!
 * @brief   Initialization of control GPIOs according to measurement type
 * @param   cal Calibration
 */
static void channelGpioInit(FlashCalibration *cal) {
    stmGpioInit(&CH_Ctrl[0], CH1_Ctrl_GPIO_Port, CH1_Ctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&CH_Ctrl[1], CH2_Ctrl_GPIO_Port, CH2_Ctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&CH_Ctrl[2], CH3_Ctrl_GPIO_Port, CH3_Ctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&CH_Ctrl[3], CH4_Ctrl_GPIO_Port, CH4_Ctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&CH_Ctrl[4], CH5_Ctrl_GPIO_Port, CH5_Ctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&CH_Ctrl[5], CH6_Ctrl_GPIO_Port, CH6_Ctrl_Pin, STM_GPIO_OUTPUT);

    for (int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        stmSetGpio(CH_Ctrl[i], cal->measurementType[i]);
    }
}

/*!
 * @brief   If nothing is stored in FLASH use default values
 * @param   cal Calibration
 */
static void setDefaultCalibration(FlashCalibration *cal) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
        /* Default "Sensor calibration" simply outputs voltage */
        if (i < NO_CALIBRATION_CHANNELS) {
            cal->sensorCalVal[i * 2]     = 1.0;
            cal->sensorCalVal[i * 2 + 1] = 0;
        }

        // The voltage divider prior to the ADC is made such that 5.112V becomes 3.33V i.e. above
        // the measurement range. This port calibration accounts for this such that 5.112V becomes
        // exactly 3.3V at the ADC input.
        cal->portCalVal[i]      = PORTCALVAL_DEFAULT;
        cal->measurementType[i] = 0;
    }
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Calibration function that adjust the raw ADC values given a precise input voltage as
 * reference
 * @param   noOfCalibrations Number of calibrations
 * @param   calibrations CA library calibration structure
 * @param   cal Calibration
 * @param   ADCMeansRaw Array of ADC means
 * @param   calSize Calibration size
 */
void calibrateBoard(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                    float *ADCMeansRaw, uint32_t calSize) {
    for (int i = 0; i < noOfCalibrations; i++) {
        // Channel to be calibrated
        if (calibrations[i].port <= 0 || calibrations[i].port > NO_CALIBRATION_CHANNELS) {
            continue;
        }
        const int channel = calibrations[i].port - 1;

        // Current input voltage on channel to calibrate against
        if (calibrations[i].alpha < 0 || calibrations[i].alpha >= MAX_VIN) {
            continue;
        }

        const float Vinput       = calibrations[i].alpha;
        float targetADC          = Vinput * ADC_MAX / MAX_VIN;
        cal->portCalVal[channel] = (targetADC / ADCMeansRaw[channel]);
    }
    // Calibrations are stored in flash
    calibrationRW(true, cal, calSize);
}

/*!
 * @brief   Calibration function that sets the sensor calibrations
 * @param   noOfCalibrations Number of calibrations
 * @param   calibrations CA library calibration structure
 * @param   cal Calibration
 * @param   calSize Calibration size
 */
void calibrateSensor(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                     uint32_t calSize) {
    for (int i = 0; i < noOfCalibrations; i++) {
        // Channel to be calibrated
        if (calibrations[i].port <= 0 || calibrations[i].port > NO_CALIBRATION_CHANNELS) {
            continue;
        }

        // Make sure the measurement type has been explicitly set
        if (calibrations[i].threshold != 0 && calibrations[i].threshold != 1) {
            continue;
        }

        const int channel                  = calibrations[i].port - 1;
        cal->sensorCalVal[channel * 2]     = calibrations[i].alpha;
        cal->sensorCalVal[channel * 2 + 1] = calibrations[i].beta;
        cal->measurementType[channel]      = calibrations->threshold;
        setMeasurementType(channel, calibrations->threshold);
    }
    // Calibrations are stored in flash
    calibrationRW(true, cal, calSize);
}

/*!
 * @brief   Calibration initialization function
 * @param   hcrc CRC handler
 * @param   cal Calibration
 * @param   size Calibration size
 */
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size) {
    hcrc_ = hcrc;

    // If calibration value is not stored in FLASH use default calibration
    if (readFromFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t *)cal, size) != 0) {
        setDefaultCalibration(cal);
    }

    // Initialise channels to measure current or voltage
    channelGpioInit(cal);
}

/*!
 * @brief   Read/write calibration
 * @param   write True if writing
 * @param   cal Calibration
 * @param   size Calibration size
 */
void calibrationRW(bool write, FlashCalibration *cal, uint32_t size) {
    if (write) {
        if (writeToFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t *)cal, size) != 0) {
            USBnprintf("Calibration was not stored in FLASH");
        }
    }
    else {
        char buf[300];
        int len = 0;
        len += snprintf(&buf[len], sizeof(buf) - len, "Calibration: CAL");
        for (int ch = 0; ch < NO_CALIBRATION_CHANNELS; ch++) {
            len += snprintf(&buf[len], sizeof(buf) - len, " %d,%.10f,%.10f,%d", ch + 1,
                            cal->sensorCalVal[ch * 2], cal->sensorCalVal[ch * 2 + 1],
                            cal->measurementType[ch]);
        }
        len += snprintf(&buf[len], sizeof(buf) - len, "\r\n");
        writeUSB(buf, len);
    }
}