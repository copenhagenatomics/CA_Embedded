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
#include "USBprint.h"
#include "calibration.h"
#include "vl53l1_api.h"
#include "pressure.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t) & _FlashAddrCal)

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void setDefaultCalibration(FlashCalibration *cal, VL53L1_DEV dev_p);
static const CACalibration* getCalFromIdx(int noOfCalibrations, const CACalibration* calibrations, int port);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

CRC_HandleTypeDef *hcrc_ = NULL;
static int cal_last_step = 0;

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   If nothing is stored in FLASH use default values
 * @param   cal Calibration
 */
static void setDefaultCalibration(FlashCalibration *cal, VL53L1_DEV dev_p) {
    /* Retrieve CAL data from device, and copy it to the local flash object, ready for saving */
    VL53L1_CalibrationData_t calib_data;
    VL53L1_Error status = VL53L1_GetCalibrationData(dev_p, &calib_data);
    if(status != 0) {bsSetFieldRange((uint8_t)status, VL53_STATUS_Msk);}

    memcpy(&cal->cal_data, &calib_data.customer, sizeof(VL53L1_CustomerNvmManaged_t));
}

static const CACalibration* getCalFromIdx(int noOfCalibrations, const CACalibration* calibrations, int port) {
    const CACalibration* return_p = NULL;
    for(int i = 0; i < noOfCalibrations; i++) {
        if(calibrations[i].port == port) {
            return_p = calibrations + i;
            break;
        }
    }
    return return_p;
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Calibration function that sets the sensor calibrations
 * @param   noOfCalibrations Number of calibrations
 * @param   calibrations CA library calibration structure
 * @param   cal Calibration
 * @param   calSize Calibration size
 */
void calibrateSensor(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration *cal,
                     uint32_t calSize, VL53L1_DEV dev_p) {

    /* Get the cal value of the next step */
    int next_step = cal_last_step + 1;
    const CACalibration* local_cal = getCalFromIdx(noOfCalibrations, calibrations, next_step);

    VL53L1_Error status;
    if(local_cal) {
        switch(next_step) {
            case 1: status = VL53L1_PerformRefSpadManagement(dev_p);
                    break;
            case 2: status = VL53L1_PerformOffsetSimpleCalibration(dev_p, (int32_t)local_cal->alpha);
                    break;
            case 3: status = VL53L1_PerformSingleTargetXTalkCalibration(dev_p, (int32_t)local_cal->alpha);
                    break;
            default:USBnprintf("CAL ordering error, restart device and try again from the beginning");
                    status = -1;
        }

        if(status != 0) {
            bsSetFieldRange((uint8_t)status, VL53_STATUS_Msk);
            cal_last_step = 0;
        }
        else {
            cal_last_step = next_step;
        }
    }
    else {
        USBnprintf("CAL step %d not found in port list", next_step);
    }
}

/*!
 * @brief   Calibration initialization function
 * @param   hcrc CRC handler
 * @param   cal Calibration
 * @param   size Calibration size
 */
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration *cal, uint32_t size, VL53L1_DEV dev_p) {
    hcrc_ = hcrc;

    // If calibration value is not stored in FLASH use default calibration
    if (readFromFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t *)cal, size) != 0) {
        setDefaultCalibration(cal, dev_p);
    }
    else {
        /* Modify on-device calibration data with saved values */
        VL53L1_CalibrationData_t calib_data;
        VL53L1_Error status = VL53L1_GetCalibrationData(dev_p, &calib_data);
        if(status != 0) {bsSetFieldRange((uint8_t)status, VL53_STATUS_Msk);}
        memcpy(&calib_data.customer, &cal->cal_data, sizeof(VL53L1_CustomerNvmManaged_t));
        status = VL53L1_SetCalibrationData(dev_p, &calib_data);
        if(status != 0) {bsSetFieldRange((uint8_t)status, VL53_STATUS_Msk);}
    }
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
        CA_SNPRINTF(buf, len, "Calibration: \r\n");
        uint8_t* cal_data_uint8 = (uint8_t*)&cal->cal_data;
        int lines = sizeof(VL53L1_CustomerNvmManaged_t) / 8;

        for (int l = 0; l <= lines; l++) {
            int remaining = sizeof(VL53L1_CustomerNvmManaged_t) - (8 * l);
            int line_len = remaining > 8 ? 8 : remaining;
            //CA_SNPRINTF(buf, len, "%d, ", line_len);
            for(int i = 0; i < line_len; i++) {
                CA_SNPRINTF(buf, len, "0x%02x ", cal_data_uint8[8 *  l + i]);
            }
            CA_SNPRINTF(buf, len, "\r\n");
        }
        writeUSB(buf, len);
    }
}