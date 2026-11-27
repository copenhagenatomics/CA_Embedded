/*!
 * @file    calibration.c
 * @brief   This file contains the calibration functions
 * @date    25/11/2026
 * @author  Timothé Dodin
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "FLASH_readwrite.h"
#include "Temperature.h"
#include "USBprint.h"
#include "calibration.h"
#include "stm32f4xx_hal.h"
#include "temperatureMeas.h"

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t)&_FlashAddrCal)

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// Legacy implementation was using linear approximations instead of full polynomial
// https://datasheets.maximintegrated.com/en/ds/MAX31855.pdf
#define TYPE_J_DELTA    0.000057953  // [V/degC] - Sensitivity for type J
#define TYPE_J_CJ_DELTA 0.000052136  // [V/degC] - Sensitivity at cold-junction for type J
#define TYPE_K_DELTA    0.000041276  // [V/degC] - Sensitivity for type K
#define TYPE_K_CJ_DELTA 0.00004073   // [V/degC] - Sensitivity at cold-junction for type K

// Relative tolerance when recognizing the linear sensitivities of the type K and J
#define LINEAR_CAL_TOLERANCE 1e-3f

typedef struct _calDef {
    float min;         // Minimum value
    float max;         // Maximum value
    float defaultVal;  // Default value
} calDef_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static bool setCal(const calDef_t* calDef, float* calPointer, float newValue);
static void setDefaultCal(FlashCalibration_t* cal, bool rtd);
static bool isNear(float value, float reference);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CRC_HandleTypeDef* hcrc_ = NULL;

static char buffer[400];  // Print buffer

static const calDef_t LEAD_RES_DEF = {0.0f, 100.0f, 0.0f};  // [ohm]

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Applies calibration after checking it is in range
 * @param calDef Pointer to calibration definition
 * @param calPointer Pointer to the calibration value
 * @param newValue Updated value
 * @return 1 if calibration has been set
 */
static bool setCal(const calDef_t* calDef, float* calPointer, float newValue) {
    if ((newValue >= calDef->min) && (newValue <= calDef->max)) {
        *calPointer = newValue;
        return true;
    }
    return false;
}

/*!
 * @brief Applies default initialization
 * @param cal Pointer to the board calibration structure
 * @param rtd RTD board sub type
 */
static void setDefaultCal(FlashCalibration_t* cal, bool rtd) {
    for (int i = 0; i < NO_PORTS; i++) {
        cal->port[i].type    = rtd ? PT_100 : TYPE_K;
        cal->port[i].leadRes = LEAD_RES_DEF.defaultVal;
    }
}

/*!
 * @brief Checks that a value is equal to a reference, within LINEAR_CAL_TOLERANCE
 * @param value Value to check
 * @param reference Reference value
 * @return True if value is close to reference
 */
static bool isNear(float value, float reference) {
    return fabsf(value - reference) <= LINEAR_CAL_TOLERANCE * fabsf(reference);
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Calibration function
 * @param noOfCalibrations Number of calibrations
 * @param calibrations Pointer to the CA calibration structure
 * @param cal Pointer to the board calibration structure
 * @param rtd RTD board sub type
 */
void calibrate(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration_t* cal,
               bool rtd) {
    /*
        Calibration commands:
        -------------------------------------------------------------------------
        Thermocouple boards (subtype 0)

        Legacy commands for type K or J
        CAL 1,0.000041276,0.00004073      - Type K
        ...
        CAL 10,0.000057953,0.000052136    - Type J

        Sets the type of thermocouple (0 = B, 1 = E, 2 = J, 3 = K, 4 = N, 5 = R, 6 = S, 7 = T)
        CAL 1,0,0,<type>
        ...
        CAL 10,0,0,<type>

        -------------------------------------------------------------------------
        RTD boards (subtype 1 and 2)

        Sets the type of RTD sensor (8 = Pt100, 9 = Pt1000)
        and the lead resistance to compensate for
        CAL 1,<leadRes>,0,<type>
        ...
        CAL 10,<leadRes>,0,<type>
        -------------------------------------------------------------------------
    */

    for (int i = 0; i < noOfCalibrations; i++) {
        const CACalibration* c = &calibrations[i];
        bool calOk             = false;  // Is the calibration valid ?

        if ((c->port >= 1) && (c->port <= NO_PORTS)) {
            // Legacy calibration for type K and J
            if ((c->noOfValues == 2) && !rtd) {
                if (isNear(c->alpha, TYPE_K_DELTA) && isNear(c->beta, TYPE_K_CJ_DELTA)) {
                    cal->port[c->port - 1].type    = TYPE_K;
                    cal->port[c->port - 1].leadRes = 0.0f;
                    calOk                          = true;
                }
                else if (isNear(c->alpha, TYPE_J_DELTA) && isNear(c->beta, TYPE_J_CJ_DELTA)) {
                    cal->port[c->port - 1].type    = TYPE_J;
                    cal->port[c->port - 1].leadRes = 0.0f;
                    calOk                          = true;
                }
            }
            // New calibration format using the sensor type (integer)
            else if ((c->noOfValues == 3) && isfinite(c->gamma) &&
                     ((float)c->gamma == roundf((float)c->gamma))) {
                int type = (int)roundf((float)c->gamma);
                // Only allows thermocouples for the correct sub type
                if ((type >= TYPE_B) && (type <= TYPE_T) && !rtd) {
                    if ((c->alpha == 0.0) && (c->beta == 0.0)) {
                        cal->port[c->port - 1].type    = type;
                        cal->port[c->port - 1].leadRes = 0.0f;
                        calOk                          = true;
                    }
                }
                // Only allows RTDs for the correct sub type
                else if ((type >= PT_100) && (type <= PT_1000) && rtd) {
                    if (c->beta == 0.0 &&
                        setCal(&LEAD_RES_DEF, &cal->port[c->port - 1].leadRes, c->alpha)) {
                        cal->port[c->port - 1].type = type;
                        calOk                       = true;
                    }
                }
            }
        }
        USBnprintf("CAL %d %s\r\n", c->port, calOk ? "success" : "failed");
    }
}

/*!
 * @brief Calibration initialization function
 * @param hcrc Pointer to the CRC handler
 * @param cal Pointer to the board calibration structure
 * @param size Size of the calibration
 * @param rtd RTD board sub type
 */
void calibrationInit(CRC_HandleTypeDef* hcrc, FlashCalibration_t* cal, uint32_t size, bool rtd) {
    hcrc_ = hcrc;

    if (readFromFlashCRC(hcrc, (uint32_t)FLASH_ADDR_CAL, (uint8_t*)cal, size) != 0) {
        setDefaultCal(cal, rtd);
        // Write default calibration to flash if no valid calibration is found
        writeCalToFlash(cal, size);
    }
}

/*!
 * @brief Prints provided calibration
 * @param cal Pointer to the board calibration
 */
void printCal(const FlashCalibration_t* cal) {
    int len = 0;

    CA_SNPRINTF(buffer, len, "Calibration: CAL");
    for (int i = 0; i < NO_PORTS; i++) {
        CA_SNPRINTF(buffer, len, " %d,%.2f,0,%d", i + 1, cal->port[i].leadRes, cal->port[i].type);
    }
    CA_SNPRINTF(buffer, len, "\r\n");

    writeUSB(buffer, len);
}

/*!
 * @brief Reads calibration from flash
 */
void readCalFromFlash() {
    FlashCalibration_t cal;

    if (readFromFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t*)&cal, sizeof(cal)) != 0) {
        USBnprintf("CAL m failed\r\n");
    }
    else {
        printCal(&cal);
    }
}

/*!
 * @brief Writes calibration into flash
 * @param cal Pointer to the board calibration
 * @param size Calibration size
 */
void writeCalToFlash(const FlashCalibration_t* cal, uint32_t size) {
    int ret = writeToFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t*)cal, size);
    USBnprintf("CAL w %s\r\n", (ret == 0) ? "success" : "failed");
}
