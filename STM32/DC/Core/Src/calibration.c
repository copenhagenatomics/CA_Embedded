/*!
 * @file    calibration.c
 * @brief   This file contains the calibration functions used by DC
 * @date    23/11/2026
 * @author  Timothé Dodin
 */

#include "calibration.h"
#include "FLASH_readwrite.h"
#include "USBprint.h"

// Extern value defined in .ld linker script
extern uint32_t _FlashAddrCal;  // Starting address of calibration values in FLASH
#define FLASH_ADDR_CAL ((uintptr_t)&_FlashAddrCal)

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

typedef struct _calDef {
    float min;         // Minimum value
    float max;         // Maximum value
    float defaultVal;  // Default value
} calDef_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static bool setCal(const calDef_t* calDef, float* calPointer, float newValue);
static void setDefaultCal(FlashCalibration_t* cal);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CRC_HandleTypeDef* hcrc_ = NULL;

static char buffer[300];  // Print buffer

static const calDef_t CUR_LIM_DEF = {0, MAX_MEASURABLE_CURRENT,
                                     MAX_MEASURABLE_CURRENT - 0.25f};  // [A]

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
 */
static void setDefaultCal(FlashCalibration_t* cal) {
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        cal->currentLimits[i] = CUR_LIM_DEF.defaultVal;
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Calibration function
 * @param noOfCalibrations Number of calibrations
 * @param calibrations Pointer to the CA calibration structure
 * @param cal Pointer to the board calibration structure
 */
void calibrate(int noOfCalibrations, const CACalibration* calibrations, FlashCalibration_t* cal) {
    /*
        Calibration commands:
        -------------------------------------------------------------------------
        Sets the e-fuse current limit
        CAL 1,eFuseCurrent1
        ...
        CAL 6,eFuseCurrent6
        -------------------------------------------------------------------------
    */

    for (int i = 0; i < noOfCalibrations; i++) {
        const CACalibration* c = &calibrations[i];
        bool calOk             = false;  // Is the calibration valid ?

        if (c->port >= 1 && c->port <= DC_BOARD_NUM_PORTS) {
            calOk = setCal(&CUR_LIM_DEF, &cal->currentLimits[c->port - 1], c->alpha);
        }

        USBnprintf("CAL %d %s\r\n", c->port, calOk ? "success" : "failed");
    }
}

/*!
 * @brief Calibration initialization function
 * @param hcrc Pointer to the CRC handler
 * @param cal Pointer to the board calibration structure
 * @param size Size of the calibration
 */
void calibrationInit(CRC_HandleTypeDef* hcrc, FlashCalibration_t* cal, uint32_t size) {
    hcrc_ = hcrc;

    if (readFromFlashCRC(hcrc, (uint32_t)FLASH_ADDR_CAL, (uint8_t*)cal, size) != 0) {
        setDefaultCal(cal);
        // Write default calibration to flash if no valid calibration is found
        writeCalToFlash(cal, size);
    }
}

/*!
 * @brief Prints provided calibration
 * @param cal Pointer to the board calibration
 */
void printCal(FlashCalibration_t* cal) {
    int len = 0;

    CA_SNPRINTF(buffer, len, "Calibration: CAL");
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        CA_SNPRINTF(buffer, len, " %d,%.2f", i + 1, cal->currentLimits[i]);
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
void writeCalToFlash(FlashCalibration_t* cal, uint32_t size) {
    int ret = writeToFlashCRC(hcrc_, (uint32_t)FLASH_ADDR_CAL, (uint8_t*)cal, size);
    USBnprintf("CAL w %s\r\n", (ret == 0) ? "success" : "failed");
}
