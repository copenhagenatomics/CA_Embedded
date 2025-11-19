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
#include "vl53l1_api.h"
#include "array-math.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         2    // Channels: FB_5V, FB_VBUS
#define ADC_CHANNEL_BUF_SIZE 400  // 4kHz sampling rate

#define RANGE_FILTER_SAMPLES 20   // Number of samples for moving average filter

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void printHeader();
static void printPressureStatus();
static void printPressureStatusDef();
static void calibrationInfoRW(bool write);
static void calibrate(int noOfCalibrations, const CACalibration *calibrations);
static void ADCtoVolt(float *adcMeans, int noOfChannels);
static void updateBoardStatus();
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);
void initVl53();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

// array for all ADC readings, filled by DMA.
static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];  

static float volts[ADC_CHANNELS];        // port voltage readings
static float ADCMeans[ADC_CHANNELS];     // ADC mean readings adjusted with portCalVal

FlashCalibration cal;

static CAProtocolCtx caProto = {.undefined        = HALundefined,
                                .printHeader      = printHeader,
                                .printStatus      = printPressureStatus,
                                .printStatusDef   = printPressureStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = calibrate,
                                .calibrationRW    = calibrationInfoRW,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

char module_verification[200] = {0};

static I2C_HandleTypeDef* _hi2c1 = NULL;
static VL53L1_Dev_t dev;
static VL53L1_DEV dev_p      = &dev;
static VL53L1_Error status = 0;
static double rangeFiltBuf[RANGE_FILTER_SAMPLES] = {0};
static moving_avg_cbuf_t rangeFilt;

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

    len += snprintf(&buf[len], sizeof(buf) - len, "VCC is: %.2f. It should be >=5.05V \r\n", volts[0]);
    len += snprintf(&buf[len], sizeof(buf) - len, "VCC raw is: %.2f. It should be >=4.6V \r\n", volts[1]);
    len += snprintf(&buf[len], sizeof(buf) - len, "VL53 Status byte is: %d.\r\n", (int)status);

    writeUSB(buf, len);
}

/*!
 * @brief   Definition of status definition information when the 'StatusDef' command is received
 */
static void printPressureStatusDef() {
    static char buf[400] = {0};

    int len = 0;

    len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIx32 ",VBUS FB\r\n", (uint32_t)VCC_RAW_ERROR_Msk);
    len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIx32 ",5V FB\r\n", (uint32_t)VCC_ERROR_Msk);
    len += snprintf(&buf[len], sizeof(buf) - len, "0x%08" PRIx32 ",VL53 Status\r\n", (uint32_t)VL53_STATUS_Msk);
    writeUSB(buf, len);
}

/*!
 * @brief   Read or write calibration
 * @param   write True when writing
 */
static void calibrationInfoRW(bool write) { calibrationRW(write, &cal, sizeof(cal)); }

/*!
 * @brief   Applies the calibration sent by the user
 * @param   noOfCalibrations Number of calibrations
 * @param   calibrations Pointer to the calibration structure
 */
static void calibrate(int noOfCalibrations, const CACalibration *calibrations) {
    /* Cannot perform calibration while ranging is ongoing */
    status = VL53L1_StopMeasurement(dev_p);

    /* Calibration can take a second, so disable watchdog */
    __WWDG_CLK_DISABLE();

    calibrateSensor(noOfCalibrations, calibrations, &cal, sizeof(cal), dev_p);

    __WWDG_CLK_ENABLE();

    /* Re-initialise the sensor - the calibration may reset all the timing parameters */
    initVl53();
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
         *  Hence, we divide by MAX_VCC_IN (=5.49V) rather than MAX_VIN */
        volts[channel] = adcScaled * MAX_VCC_IN;
    }
}

/*!
 * @brief   Update status bits
 */
static void updateBoardStatus() {
    // Check input voltages
    (volts[0] <= MIN_VCC) ? bsSetError(VCC_ERROR_Msk) : bsClearField(VCC_ERROR_Msk);
    (volts[1] <= MIN_RAW_VCC) ? bsSetError(VCC_RAW_ERROR_Msk) : bsClearField(VCC_RAW_ERROR_Msk);
    
    setBoardVoltage(volts[0]);
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
    static float range_millimeter_filt = 0.0f;

    if (!isUsbPortOpen()) {
        return;
    }

    /* If the version is incorrect only print out status code */
    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    for (int channel = 0; channel < noOfChannels; channel++) {
        ADCMeans[channel] = ADCMean(pData, channel);
    }

    ADCtoVolt(ADCMeans, noOfChannels);

    uint8_t measurementReady = 0;
    status = VL53L1_GetMeasurementDataReady(dev_p, &measurementReady);
    
    static VL53L1_RangingMeasurementData_t rd;
    if(status==0 && measurementReady){
        (void) VL53L1_GetRangingMeasurementData(dev_p, &rd);
    }

    /* Call this every time as the device is working at 10 Hz, so there should always be a new measurement */
    status = VL53L1_ClearInterruptAndStartMeasurement(dev_p);

    bsSetFieldRange((uint8_t)status, VL53_STATUS_Msk);
    bsUpdateError(RANGING_ERROR_Msk, (rd.RangeStatus), PRESSURE_ERROR_Msk);

    range_millimeter_filt = maMean(&rangeFilt, (double)rd.RangeMilliMeter);

    USBnprintf("%d, %0.6f, %0.6f, %0.6f, 0x%08" PRIx32, 
        rd.RangeMilliMeter, range_millimeter_filt, 
        (rd.SignalRateRtnMegaCps/65536.0), (rd.AmbientRateRtnMegaCps/65336.0),
        bsGetStatus());
}

/*!
** @brief Setup VL53 sensor
*/
void initVl53() {
    status = VL53L1_SetDistanceMode(dev_p, VL53L1_DISTANCEMODE_SHORT);
    if(status != 0) goto RANGINGERROR;
    status = VL53L1_SetMeasurementTimingBudgetMicroSeconds(dev_p, 45000);
    if(status != 0) goto RANGINGERROR;
    status = VL53L1_SetInterMeasurementPeriodMilliSeconds(dev_p, 55);
    if(status != 0) goto RANGINGERROR;
    status = VL53L1_StartMeasurement(dev_p);
    if(status != 0) goto RANGINGERROR;

RANGINGERROR:
    bsSetFieldRange(status, VL53_STATUS_Msk);
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
void pressureInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc, I2C_HandleTypeDef *hi2c) {
    initCAProtocol(&caProto, usbRx);

    boardSetup(Pressure, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, PRESSURE_ERROR_Msk);

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));

    _hi2c1 = hi2c;
    dev_p->I2cHandle = _hi2c1;
    dev_p->I2cDevAddr = 0x52;
    status = VL53L1_WaitDeviceBooted(dev_p);
    status = VL53L1_DataInit(dev_p);
    status = VL53L1_StaticInit(dev_p);
    initVl53();

    calibrationInit(hcrc, &cal, sizeof(cal), dev_p);

    maInit(&rangeFilt, rangeFiltBuf, RANGE_FILTER_SAMPLES);
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void pressureLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, module_verification);
    updateBoardStatus();
    ADCMonitorLoop(adcCallback);
}
