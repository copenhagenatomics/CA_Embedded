/*!
 * @file    calibration.c
 * @brief   This file contains the calibration functions used by SaltLeak
 * @date	22/05/2025
 * @author 	Timothé Dodin
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "FLASH_readwrite.h"
#include "USBprint.h"
#include "calibration.h"
#include "stm32f4xx_hal.h"

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t) & _FlashAddrCal)

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define NO_CALIBRATION 13  // 1 couple of calibrations per sensor + 1 for boost voltage (2*6 + 1)

#define MIN_RES_P1     6.0  // kOhm
#define MAX_RES_P1     6.4  // kOhm
#define DEFAULT_RES_P1 6.2  // kOhm - PCB ideal value

#define MIN_RES_P2     98.0   // kOhm
#define MAX_RES_P2     102.0  // kOhm
#define DEFAULT_RES_P2 100.0  // kOhm - PCB ideal value

#define MIN_RES_N1     17.6  // kOhm
#define MAX_RES_N1     18.4  // kOhm
#define DEFAULT_RES_N1 18.0  // kOhm - PCB ideal value

#define MIN_VOLT_SCALAR     0.012
#define MAX_VOLT_SCALAR     0.013
#define DEFAULT_VOLT_SCALAR 0.012443  // PCB ideal voltage divider

#define MIN_BOOST_SCALAR     0.012
#define MAX_BOOST_SCALAR     0.013
#define DEFAULT_BOOST_SCALAR 0.012443  // PCB ideal voltage divider

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void setDefaultCalibration(FlashCalibration_t *cal);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

CRC_HandleTypeDef *hcrc_ = NULL;

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

static void setDefaultCalibration(FlashCalibration_t *cal) {
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        cal->sensorCal[i].resP1   = DEFAULT_RES_P1;
        cal->sensorCal[i].resP2   = DEFAULT_RES_P2;
        cal->sensorCal[i].resN1   = DEFAULT_RES_N1;
        cal->sensorCal[i].vScalar = DEFAULT_VOLT_SCALAR;
    }
    cal->boostScalar = DEFAULT_BOOST_SCALAR;
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief PhaseMonitor calibration function
 * @param noOfCalibrations Number of calibrations
 * @param calibrations Pointer to the CA calibration structure
 * @param cal Pointer to the board calibration structure
 * @param size Size of the calibration
 */
void calibration(int noOfCalibrations, const CACalibration *calibrations, FlashCalibration_t *cal,
                 uint32_t size, float sensorVoltages[NO_OF_SENSORS], float voltageBoost) {
    /*
        Calibration command examples:
        -------------------------------------------------------------------------
        CAL 1,resP1_1,resP2_1 2,resN1_1,VoutMeasured_1     |   Sensor 1
        CAL 3,resP1_2,resP2_2 4,resN1_2,VoutMeasured_2     |   Sensor 2
        ...                                                |   ...
        CAL 11,resP1_6,resP2_6 12,resN1_6,VoutMeasured_6   |   Sensor 6
        CAL 13,VboostMeasured,0                            |   Boost measurement
        -------------------------------------------------------------------------
    */
    for (int i = 0; i < noOfCalibrations; i++) {
        if (calibrations[i].port <= 0 || calibrations[i].port > NO_CALIBRATION) {
            continue;
        }
        const int channel = calibrations[i].port - 1;
        double alpha      = calibrations[i].alpha;
        double beta       = calibrations[i].beta;

        // Vboost voltage divider calibration
        if (channel == 12) {
            float scalar = cal->boostScalar * alpha / voltageBoost;
            if (scalar >= MIN_BOOST_SCALAR && scalar <= MAX_BOOST_SCALAR) {
                cal->boostScalar = scalar;
            }
        }
        // P1 and P2 sensor resistors calibration
        else if (channel % 2 == 0) {
            uint8_t i = channel / 2;
            if (alpha >= MIN_RES_P1 && alpha <= MAX_RES_P1) {
                cal->sensorCal[i].resP1 = alpha;
            }
            if (beta >= MIN_RES_P2 && beta <= MAX_RES_P2) {
                cal->sensorCal[i].resP2 = beta;
            }
        }
        // N1 sensor resistor and voltage divider calibration
        else {
            uint8_t i = (channel - 1) / 2;
            if (alpha >= MIN_RES_N1 && alpha <= MAX_RES_N1) {
                cal->sensorCal[i].resN1 = alpha;
            }
            float scalar = cal->sensorCal[i].vScalar * beta / sensorVoltages[i];
            if (scalar >= MIN_VOLT_SCALAR && scalar <= MAX_VOLT_SCALAR) {
                cal->sensorCal[i].vScalar = scalar;
            }
        }
    }

    calibrationRW(true, cal, size);
}

/*!
 * @brief PhaseMonitor calibration initialization function
 * @param hcrc Pointer to the CRC handler
 * @param cal Pointer to the board calibration structure
 * @param size Size of the calibration
 */
void calibrationInit(CRC_HandleTypeDef *hcrc, FlashCalibration_t *cal, uint32_t size) {
    hcrc_ = hcrc;

    if (readFromFlashCRC(hcrc, (uint32_t)FLASH_ADDR_CAL, (uint8_t *)cal, size) != 0) {
        setDefaultCalibration(cal);
    }
}

/*!
 * @brief Read and write calibration function
 * @param write Write into the flash memory
 * @param cal Pointer to the board calibration structure
 * @param size Size of the calibration
 */
void calibrationRW(bool write, FlashCalibration_t *cal, uint32_t size) {
    if (write) {
        if (writeToFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t *)cal, size) != 0) {
            USBnprintf("Calibration was not stored in FLASH");
        }
    }
    else {
        char buf[300];
        int len = 0;

        len += snprintf(&buf[len], sizeof(buf) - len, "Calibration: CAL");
        for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
            len += snprintf(&buf[len], sizeof(buf) - len, " %d,%0.3f,%0.3f", 2 * i + 1,
                            cal->sensorCal[i].resP1, cal->sensorCal[i].resP2);
            len += snprintf(&buf[len], sizeof(buf) - len, " %d,%0.3f,%0.3f", 2 * (i + 1),
                            cal->sensorCal[i].resN1, cal->sensorCal[i].vScalar * 1e3);
        }
        len += snprintf(&buf[len], sizeof(buf) - len, " 13,%0.3f,0", cal->boostScalar * 1e3);
        len += snprintf(&buf[len], sizeof(buf) - len, "\r\n");

        writeUSB(buf, len);
    }
}
