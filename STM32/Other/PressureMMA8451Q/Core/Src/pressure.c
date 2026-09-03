/*!
 * @file    pressure.c
 * @brief   This file contains the main program of Pressure
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#include <inttypes.h>
#include <stdio.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "MMA8451Q.h"
#include "USBprint.h"
#include "pcbversion.h"
#include "pressure.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* The ADC is only used to regulate the printout frequency, its data is not used for anything.
   A single dummy channel is sampled. */
#define ADC_CHANNELS         1
#define ADC_CHANNEL_BUF_SIZE 400  // 4kHz sampling rate

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void printHeader();
static void printPressureStatus();
static void printPressureStatusDef();
static void updateBoardStatus();
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE *
                         2];  // Unused, only paces the printout via DMA half/full callbacks.

static mma8451q_t accel;       // Accelerometer device
static int accelInitResult;    // TEMPORARY DEBUG: return code from mma8451q_init()

static CAProtocolCtx caProto = {.undefined        = HALundefined,
                                .printHeader      = printHeader,
                                .printStatus      = printPressureStatus,
                                .printStatusDef   = printPressureStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = NULL,
                                .calibrationRW    = NULL,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Definition of what is printed when the 'Serial' command is received
 */
static void printHeader() {
    CAPrintHeader();
}

/*!
 * @brief   Definition of status information when the 'Status' command is received
 */
static void printPressureStatus() {
    static char buf[100] = {0};
    int len              = 0;

    if (bsGetField(ACCEL_ERROR_Msk)) {
        CA_SNPRINTF(buf, len, "Accelerometer is not responding.\r\n");
    }
    else {
        CA_SNPRINTF(buf, len, "Accelerometer is operating normally.\r\n");
    }
    writeUSB(buf, len);
}

/*!
 * @brief   Definition of status definition information when the 'StatusDef' command is received
 */
static void printPressureStatusDef() {
    static char buf[100] = {0};
    int len              = 0;

    CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Accelerometer communication error\r\n",
                (uint32_t)ACCEL_ERROR_Msk);

    writeUSB(buf, len);
}

/*!
 * @brief   Update status bits
 */
static void updateBoardStatus() {
    bsUpdateError(ACCEL_ERROR_Msk, accel.error, PRESSURE_ERROR_Msk);
}

/*!
 * @brief   Callback called when ADC circular buffer is half full or full. The ADC data itself is
 *          ignored, it is only used to regulate the printout frequency. Reads and prints the
 *          latest X, Y, Z acceleration from the MMA8451Q accelerometer.
 * @param   pData ADC buffer (unused)
 * @param   noOfChannels Number of ADC channels (unused)
 * @param   noOfSamples Number of ADC samples in buffer per channel (unused)
 */
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples) {
    if (!isUsbPortOpen()) {
        return;
    }

    mma8451q_loop(&accel);
    updateBoardStatus();

    /* If the version is incorrect only print out status code */
    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    USBnprintf("%0.6f, %0.6f, %0.6f, 0x%08" PRIx32 "\r\n",
               accel.data.x, accel.data.y, accel.data.z, bsGetStatus());
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Initialization function
 * @note    Should be called once before pressureLoop()
 * @param   hadc Pointer to ADC handler
 * @param   hi2c Pointer to I2C handler
 */
void pressureInit(ADC_HandleTypeDef *hadc, I2C_HandleTypeDef *hi2c) {
    initCAProtocol(&caProto, usbRx);

    boardSetup(Pressure, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, PRESSURE_ERROR_Msk);

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));

    accelInitResult = mma8451q_init(&accel, hi2c, MMA8451Q_I2C_ADDR_1);
    if (accelInitResult != 0) {
        bsSetError(ACCEL_ERROR_Msk);
    }
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void pressureLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(adcCallback);
}
