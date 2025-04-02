/*!
 * @file    pressure.c
 * @brief   This file contains the main program of Pressure
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "pcbversion.h"
#include "pressure.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         8    // Channels: Pressure 1 - 6, FB_5V, FB_VBUS
#define ADC_CHANNEL_BUF_SIZE 400  // 4kHz sampling rate

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void printHeader();
static void printPressureStatus();
static void printPressureStatusDef();
static void calibrationInfoRW(bool write);
static void logMode(int port);
static void calibrateSensorOrBoard(int noOfCalibrations, const CACalibration *calibrations);
static void ADCtoPressure(float *adcMeans, int noOfChannels);
static void ADCtoVolt(float *adcMeans, int noOfChannels);
static void printPorts(float *portValues);
static void updateBoardStatus();
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE *
                         2];  // array for all ADC readings, filled by DMA.

static float pressure[ADC_CHANNELS];     // port pressure readings
static float volts[ADC_CHANNELS];        // port voltage readings
static float ADCMeans[ADC_CHANNELS];     // ADC mean readings adjusted with portCalVal
static float ADCMeansRaw[ADC_CHANNELS];  // ADC mean readings

FlashCalibration cal;

static int loggingMode = 0;

static CAProtocolCtx caProto = {.undefined        = HALundefined,
                                .printHeader      = printHeader,
                                .printStatus      = printPressureStatus,
                                .printStatusDef   = printPressureStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = calibrateSensorOrBoard,
                                .calibrationRW    = calibrationInfoRW,
                                .logging          = logMode,
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
    calibrationInfoRW(false);
}

/*!
 * @brief   Definition of status information when the 'Status' command is received
 */
static void printPressureStatus() {
    static char buf[600] = {0};
    int len              = 0;

    for (int i = 0; i < 6; i++) {
        if (bsGetField(PORT_MEASUREMENT_TYPE(i))) {
            len += snprintf(&buf[len], sizeof(buf) - len, "Port %d measures current [4-20mA]\r\n",
                            i + 1);
        }
        else {
            len += snprintf(&buf[len], sizeof(buf) - len, "Port %d measures voltage [0-5V]\r\n",
                            i + 1);
        }
    }

    if (bsGetField(VCC_ERROR_Msk)) {
        len += snprintf(&buf[len], sizeof(buf) - len, "VCC is: %.2f. It should be >=5.05V \r\n",
                        volts[6]);
    }

    if (bsGetField(VCC_RAW_ERROR_Msk)) {
        len += snprintf(&buf[len], sizeof(buf) - len, "VCC raw is: %.2f. It should be >=4.6V \r\n",
                        volts[7]);
    }
    writeUSB(buf, len);
}

/*!
 * @brief   Definition of status definition information when the 'StatusDef' command is received
 */
static void printPressureStatusDef() {
    static char buf[400] = {0};

    int len = 0;

    len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIx32 ",VBUS FB\r\n",
                    (uint32_t)VCC_RAW_ERROR_Msk);
    len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIx32 ",5V FB\r\n",
                    (uint32_t)VCC_ERROR_Msk);

    for (int i = 5; i >= 0; i--) {
        len += snprintf(&buf[len], sizeof(buf) - len,
                        "0x%08" PRIx32 ",Port %d measure type [Voltage/Current]\r\n",
                        (uint32_t)PORT_MEASUREMENT_TYPE(i), i + 1);
    }

    writeUSB(buf, len);
}

/*!
 * @brief   Read or write calibration
 * @param   write True when writing
 */
static void calibrationInfoRW(bool write) { calibrationRW(write, &cal, sizeof(cal)); }

/*!
 * @brief   Switch between the different LOG modes
 * @param   port LOG mode
 */
static void logMode(int port) {
    if (port >= 0 && port < 3) {
        loggingMode = port;
    }
}

/*!
 * @brief   Applies the calibration sent by the user
 * @param   noOfCalibrations Number of calibrations
 * @param   calibrations Pointer to the calibration structure
 */
static void calibrateSensorOrBoard(int noOfCalibrations, const CACalibration *calibrations) {
    // Overload of threshold value to switch between sensor calibration
    // and board calibration mode.
    //		calibrations->threshold == 0 -> sensor calibration
    // 		calibrations->threshold == 1 -> board port calibration
    if (calibrations->threshold == 2) {
        if (loggingMode != 1) {
            USBnprintf("To calibrate board, first enter voltLogging mode by typing: 'LOG p1'");
            return;
        }
        calibrateBoard(noOfCalibrations, calibrations, &cal, ADCMeansRaw, sizeof(cal));
    }
    else {
        calibrateSensor(noOfCalibrations, calibrations, &cal, sizeof(cal));
    }
}

/*!
 * @brief   Convert from ADC means to pressure via its calibration function
 * @param   adcMeans Array of ADC means
 * @param   noOfChannels Number of ADC channels to convert
 */
static void ADCtoPressure(float *adcMeans, int noOfChannels) {
    /*
    cal.sensorCalVal[channel*2]     - Scale assuming [0V, 5V] range
    VOLTAGE_SCALING                 - To go to real [0V, 5.112V] range
    cal.sensorCalVal[channel*2+1]   - Pressure bias
    */
    for (int channel = 0; channel < noOfChannels; channel++) {
        pressure[channel] = adcMeans[channel] * VOLTAGE_SCALING * cal.sensorCalVal[channel * 2] +
                            cal.sensorCalVal[channel * 2 + 1];
    }
}

/*!
 * @brief   Convert from ADC means to voltages
 * @param   adcMeans Array of ADC means
 * @param   noOfChannels Number of ADC channels to convert
 */
static void ADCtoVolt(float *adcMeans, int noOfChannels) {
    // Convert from ADC means to volt
    for (int channel = 0; channel < noOfChannels; channel++) {
        float adcScaled = adcMeans[channel] / (ADC_MAX + 1);

        /*  VCC and VCC raw have a different voltage divider network to enable measuring above 5V.
         *  Hence, we divide by MAX_VCC_IN (=5.49V) rather than MAX_VIN
         */

        // Current measurement across shunt resistor without voltage divider
        if (cal.measurementType[channel]) {
            volts[channel] = (channel <= 5) ? adcScaled * V_REF : adcScaled * MAX_VCC_IN;
        }
        // Voltage measurement with voltage divider
        else {
            volts[channel] = (channel <= 5) ? adcScaled * MAX_VIN : adcScaled * MAX_VCC_IN;
        }
    }
}

/*!
 * @brief   Print values on USB
 * @param   portValues Array of values to plot
 */
static void printPorts(float *portValues) {
    USBnprintf("%0.6f, %0.6f, %0.6f, %0.6f, %0.6f, %0.6f, 0x%08" PRIx32, *portValues,
               *(portValues + 1), *(portValues + 2), *(portValues + 3), *(portValues + 4),
               *(portValues + 5), bsGetStatus());
}

/*!
 * @brief   Update status bits
 */
static void updateBoardStatus() {
    // Check whether a port is set to measure voltage (=0) or current (=1)
    for (int i = 0; i < 6; i++) {
        (cal.measurementType[i]) ? bsSetField(PORT_MEASUREMENT_TYPE(i))
                                 : bsClearField(PORT_MEASUREMENT_TYPE(i));
    }

    // Check input voltages
    (volts[6] <= MIN_VCC) ? bsSetError(VCC_ERROR_Msk) : bsClearField(VCC_ERROR_Msk);
    (volts[7] <= MIN_RAW_VCC) ? bsSetError(VCC_RAW_ERROR_Msk) : bsClearField(VCC_RAW_ERROR_Msk);

    setBoardVoltage(volts[6]);
    if (bsGetField(VCC_ERROR_Msk) || bsGetField(VCC_RAW_ERROR_Msk)) {
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else {
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }

    bsClearError(PRESSURE_ERROR_Msk);
}

/*!
 * @brief   Callback called when ADC circular buffer is half full or full
 * @param   pData ADC buffer
 * @param   noOfChannels Number of ADC channels
 * @param   noOfSamples Number of ADC samples in buffer per channel
 */
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples) {
    if (!isUsbPortOpen()) {
        return;
    }

    /* If the version is incorrect only print out status code */
    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    for (int channel = 0; channel < noOfChannels; channel++) {
        ADCMeansRaw[channel] = ADCMean(pData, channel);
        ADCMeans[channel]    = ADCMeansRaw[channel] * cal.portCalVal[channel];
    }

    // We exclude the last two channels (VCC and VCC raw) when converting the ADC to pressure.
    ADCtoPressure(ADCMeans, noOfChannels - 2);
    ADCtoVolt(ADCMeans, noOfChannels);

    if (loggingMode == 0) {
        printPorts(pressure);
    }
    else if (loggingMode == 1) {
        printPorts(volts);
    }
    else if (loggingMode == 2) {
        printPorts(ADCMeans);
    }
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Initialization function
 * @note    Should be called once before pressureLoop()
 * @param   hadc Pointer to ADC handler
 * @param   hcrc Pointer to CRC handler
 */
void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc) {
    initCAProtocol(&caProto, usbRx);

    boardSetup(Pressure, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, PRESSURE_ERROR_Msk);

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));
    calibrationInit(hcrc, &cal, sizeof(cal));
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void pressureLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();
    ADCMonitorLoop(adcCallback);
}
