/*!
 * @file    analog_input.c
 * @brief   This file contains the main program of AnalogInput
 * @date    28/03/2025
 * @author  Matias, Timothé D
 */

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "pcbversion.h"
#include "analog_input.h"
#include "systemInfo.h"
#include "calibration.h"
#include "MCP4531.h"
#include "uptime.h"
#include "githash.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         8    // Channels: AnalogInput 1 - 6, FB_5V, FB_VBUS
#define ADC_CHANNEL_BUF_SIZE 400  // 4kHz sampling rate

/* Transform for addressing 6x digipots on single I2C bus */
#define I2C_MUX(x) ((uint8_t) 0x28 + ((x) & 0x7U)) 
#define DIGIPOT_MAX (pow(2, num_digipot_bits))

/* Circuit constants */
#define MEASURE_VOLTAGE_DIVIDER (2.0 / 15.0) 
#define POWER_VOLTAGE_DIVIDER   (4.3 / 134.3) 
#define AP64060_FB_VOLTAGE      (0.8)
#define MAX_VOLTAGE             (24.5)
#define CURRENT_MEASURE_RESISTOR (160.0) /* 160 Ohm shunt resistor */

#define AMPS_TO_MILLIAMPS 1000.0f

typedef struct {
    float voltage_range;
    uint16_t wiperPos;
    uint16_t wiperTarget;
    mcp4531_handle_t handle;
} digipot_t;

/***************************************************************************************************
** PRIVATE PROTOTYPE FUNCTIONS
***************************************************************************************************/

static void printHeader();
static void printAnalogInputStatus();
static void printAnalogInputStatusDef();
static void calibrationInfoRW(bool write);
static void logMode(int port);
static void calibrateSensorOrBoard(int noOfCalibrations, const CACalibration *calibrations);
static void voltsToAnalog(int noOfChannels);
static void ADCtoVolt(float *adcMeans, int noOfChannels);
static void printPorts(float *portValues);
static void updateBoardStatus();
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);
static int initDigiPots(unsigned int i);
static void analogInputCommandHandler(const char* input);
static uint16_t measureVoltageToDigipotIdx(float measure_volt);
static uint16_t powerVoltageToDigipotIdx(float power_volt);
static void forceCurrentMeasurementRange();
static void setDigipotWiper(digipot_t* digipot, unsigned int channel, uint16_t pos, bool power);
static void analogInputUptimeHandler(const char* input);
static void updateDigipotWipers();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE *
                         2];  // array for all ADC readings, filled by DMA.

static float analog_input[ADC_CHANNELS];     // port readings
static float volts[ADC_CHANNELS];        // port voltage readings
static float ADCMeans[ADC_CHANNELS];     // ADC mean readings adjusted with portCalVal
static float ADCMeansRaw[ADC_CHANNELS];  // ADC mean readings

FlashCalibration cal;

static int loggingMode = 0;

static CAProtocolCtx caProto = {.undefined        = analogInputCommandHandler,
                                .printHeader      = printHeader,
                                .printStatus      = printAnalogInputStatus,
                                .printStatusDef   = printAnalogInputStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = calibrateSensorOrBoard,
                                .calibrationRW    = calibrationInfoRW,
                                .logging          = logMode,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL,
                                .uptime           = analogInputUptimeHandler};

static digipot_t power_pots[NO_CALIBRATION_CHANNELS] = {0};
static digipot_t measure_pots[NO_CALIBRATION_CHANNELS] = {0};
static I2C_HandleTypeDef *hi2c = NULL;
static StmGpio boost_en;
static uint8_t num_digipot_bits = 7U;

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Handles the "uptime" command
*/
static void analogInputUptimeHandler(const char* input) {
    uptime_inputHandler(input, printHeader);
}

/*!
** @brief Converts a selected voltage range to digital potentiometer index
*/
static uint16_t measureVoltageToDigipotIdx(float measure_volt) {
    if(measure_volt < 0.0) {
        measure_volt = 0.0;
    }

    uint16_t idx = (uint16_t) ceil((DIGIPOT_MAX / V_REF) * MEASURE_VOLTAGE_DIVIDER * measure_volt);

    /* Note: output goes from 0 to 128 or 256 (NOT 128 - 1 or 256 - 1) */
    return idx <= DIGIPOT_MAX ? idx : DIGIPOT_MAX;
}

/*!
** @brief Converts a digital potentiometer index to a voltage range
*/
static float digipotIdxToMeasureVoltage(uint16_t idx) {
    return ((float) idx) * (V_REF / DIGIPOT_MAX) / MEASURE_VOLTAGE_DIVIDER;
}

/*!
** @brief Converts a power voltage range to digital potentiometer index
*/
static uint16_t powerVoltageToDigipotIdx(float power_volt) {
    if(power_volt < 0.0) {
        power_volt = 0.0;
    }

    uint16_t idx = (uint16_t) ((DIGIPOT_MAX / AP64060_FB_VOLTAGE) * POWER_VOLTAGE_DIVIDER * power_volt);

    /* Note: output goes from 0 to 128 or 256 (NOT 128 - 1 or 256 - 1) */
    return idx <= DIGIPOT_MAX ? idx : DIGIPOT_MAX;
}

/*!
** @brief Converts a digital potentiometer index to a voltage range
*/
static float digipotIdxToPowerVoltage(uint16_t idx) {
    return ((float) idx) * (AP64060_FB_VOLTAGE / DIGIPOT_MAX) / POWER_VOLTAGE_DIVIDER;
}

/*!
** @brief Handles communication from the serial interface
*/
static void analogInputCommandHandler(const char* input) {
    unsigned int channel = 0;
    float volt_range = 0;
    if (sscanf(input, "p%d inmax %f", &channel, &volt_range) == 2) {
        /* Only for channels within range and set to measure voltage. Current measurement is forced
        * to 20 mA max. */
        if(channel >= 1 && channel <= NO_CALIBRATION_CHANNELS && 
           cal.measurementType[channel - 1] == 0 && 
           volt_range >= 0 && volt_range <= MAX_VOLTAGE) {
            measure_pots[channel - 1].wiperTarget = measureVoltageToDigipotIdx(volt_range);
        }
        else {
            HALundefined(input);
        }
    }
    else if (sscanf(input, "p%d volt %f", &channel, &volt_range) == 2) {
        if(channel >= 1 && channel <= NO_CALIBRATION_CHANNELS && 
           volt_range >= 0 && volt_range <= MAX_VOLTAGE) {
            power_pots[channel - 1].wiperTarget = powerVoltageToDigipotIdx(volt_range);
        }
        else {
            HALundefined(input);
        }
    }
    else {
        HALundefined(input);
    }
}

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
static void printAnalogInputStatus() {
    static char buf[600] = {0};
    int len              = 0;

    for (int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        if(bsGetField(I2C_ERROR_Msk(i))) {
            CA_SNPRINTF(buf, len, "Error on I2C Channel %d\r\n", i + 1);
        }

        if (bsGetField(PORT_MEASUREMENT_TYPE(i))) {
            CA_SNPRINTF(buf, len, "Port %d measures current [4-20mA]\r\n", i + 1);
        }
        else {
            CA_SNPRINTF(buf, len, "Port %d measures voltage [0-%.0fV]\r\n", i + 1, 
                        measure_pots[i].voltage_range);
        }
    }

    if (bsGetField(VCC_28V_ERROR_Msk)) {
        CA_SNPRINTF(buf, len, "VCC_28V is: %.2f. It should be >=%.2fV \r\n", volts[ADC_CHANNEL_28V], 
                    MIN_28V);
    }

    if (bsGetField(VCC_RAW_ERROR_Msk)) {
        CA_SNPRINTF(buf, len, "VBUS is: %.2f. It should be >=%.1fV \r\n", 
                    volts[ADC_CHANNEL_VBUS], MIN_VBUS);
    }
    writeUSB(buf, len);
}

/*!
 * @brief   Definition of status definition information when the 'StatusDef' command is received
 */
static void printAnalogInputStatusDef() {
    static char buf[600] = {0};

    int len = 0;

    for(int i = NO_CALIBRATION_CHANNELS - 1; i >= 0; i--) {
        CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Error I2C Channel %d\r\n", 
                    (uint32_t)I2C_ERROR_Msk(i), i + 1);
    }

    CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",VBUS Error\r\n", (uint32_t)VCC_RAW_ERROR_Msk);
    CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",VCC_28V Error\r\n", (uint32_t)VCC_28V_ERROR_Msk);

    for (int i = 5; i >= 0; i--) {
        CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Port %d measure type [Voltage/Current]\r\n",
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
    //   calibrations->threshold == 0 -> sensor calibration
    //   calibrations->threshold == 2 -> board port calibration
    if (calibrations->threshold == 2) {
        if (loggingMode != 1) {
            USBnprintf("To calibrate board, first enter voltLogging mode by typing: 'LOG p1'");
            return;
        }

        /* Note: Assume the same potentiometer setup is applied across the entire board during 
        ** calibration */
        calibrateBoard(noOfCalibrations, calibrations, &cal, ADCMeansRaw, sizeof(cal), 
                       digipotIdxToMeasureVoltage(measure_pots[0].wiperPos));
    }
    else {
        calibrateSensor(noOfCalibrations, calibrations, &cal, sizeof(cal));
        forceCurrentMeasurementRange();
    }
}

/*!
 * @brief   Convert from ADC means to voltages. 
 * @param   adcMeans Array of ADC means (already calibrated)
 * @param   noOfChannels Number of ADC channels to convert
 */
static void ADCtoVolt(float *adcMeans, int noOfChannels) {
    // Convert from ADC means to volt
    for (int channel = 0; channel < noOfChannels; channel++) {
        float adcScaled = adcMeans[channel] / ADC_MAX;

        /* Calculate voltage first */
        if(channel < NO_CALIBRATION_CHANNELS) {
            volts[channel] = adcScaled * measure_pots[channel].voltage_range;
        }
        else if (channel == ADC_CHANNEL_28V) {
            volts[channel] = adcScaled * MAX_28V_IN;
        }
        else { /* channel == ADC_CHANNEL_VBUS */
            volts[channel] = adcScaled * MAX_VBUS_IN;
        }

        /* If doing current measurement, convert voltage to current through shunt resistor */
        if(cal.measurementType[channel]) {
            volts[channel] = AMPS_TO_MILLIAMPS * volts[channel] / CURRENT_MEASURE_RESISTOR;
        }
    }
}

/*!
 * @brief   Convert from ADC voltages to sensor analog via its calibration function
 * @param   noOfChannels Number of ADC channels to convert
 */
static void voltsToAnalog(int noOfChannels) {
    /* cal.sensorCalVal[channel*2]   - Scalar
    ** cal.sensorCalVal[channel*2+1] - Analog bias */
    for (int channel = 0; channel < noOfChannels; channel++) {
        analog_input[channel] = volts[channel] * cal.sensorCalVal[channel * 2] + 
                                cal.sensorCalVal[channel * 2 + 1];
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
    // Check all I2C channels and attempt to reconnect if one is down
    if(!bsGetField(BS_VERSION_ERROR_Msk)) {
        for (int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
            if (bsGetField(I2C_ERROR_Msk(i))) {
                if (0 == initDigiPots(i)) {
                    bsClearField(I2C_ERROR_Msk(i));
                }
            }
        }
    }

    // Check whether a port is set to measure voltage (=0) or current (=1)
    for (int i = 0; i < 6; i++) {
        bsUpdateField(PORT_MEASUREMENT_TYPE(i), cal.measurementType[i]);
    }

    // Check input voltages
    bsUpdateField(VCC_28V_ERROR_Msk, volts[ADC_CHANNEL_28V]  <= MIN_28V);
    bsUpdateField(VCC_RAW_ERROR_Msk, volts[ADC_CHANNEL_VBUS] <= MIN_VBUS);

    setBoardVoltage(volts[ADC_CHANNEL_28V]);
    if (bsGetField(VCC_28V_ERROR_Msk) || bsGetField(VCC_RAW_ERROR_Msk)) {
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else {
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }

    bsClearError(ANALOG_INPUT_ERROR_Msk);
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

    /* Apply calibration to make ADC means match calibration station (e.g. account for errors in the divider/reference voltage) */
    for (int channel = 0; channel < noOfChannels; channel++) {
        ADCMeansRaw[channel] = ADCMean(pData, channel);
        ADCMeans[channel]    = ADCMeansRaw[channel] * cal.portCalVal[channel];
    }

    // We exclude the last two channels (28V rail and VBUS) when converting the voltages to analog.
    ADCtoVolt(ADCMeans, noOfChannels);
    voltsToAnalog(noOfChannels - 2);

    if (loggingMode == 0) {
        printPorts(analog_input);
    }
    else if (loggingMode == 1) {
        printPorts(volts);
    }
    else if (loggingMode == 2) {
        printPorts(ADCMeans);
    }
}

/*!
** @brief  Initialize the digital potentiometers
**
** @param  i Index of the digital potentiometer to initialize
*/
static int initDigiPots(unsigned int i) {
    int ret = 0;
    if (0 == mcp4531_init(&measure_pots[i].handle, hi2c, I2C_MUX(i), num_digipot_bits, 0)) {
        // Set wiper to 5V range by default
        measure_pots[i].wiperTarget = measureVoltageToDigipotIdx(5.1);
        setDigipotWiper(&measure_pots[i], i, measureVoltageToDigipotIdx(5.1), false);
    }
    else {
        bsSetError(I2C_ERROR_Msk(i));
        ret = -1;
    }

    if (0 == mcp4531_init(&power_pots[i].handle, hi2c, I2C_MUX(i), num_digipot_bits, 1)) {
        // Set wiper to 5V (5.1V) power output by default
        power_pots[i].wiperTarget = powerVoltageToDigipotIdx(5.1);
        setDigipotWiper(&power_pots[i], i, powerVoltageToDigipotIdx(5.1), true);
    }
    else {
        bsSetError(I2C_ERROR_Msk(i));
        ret = -1;
    }

    return ret;
}

/*!
** @brief Set the wiper position of a digital potentiometer and update the local copy
*/
static void setDigipotWiper(digipot_t* digipot, unsigned int channel, uint16_t pos, bool power) {
    if (0 == mcp4531_setWiperPos(&digipot->handle, pos)) {
        digipot->wiperPos = pos;
        digipot->voltage_range = power ? digipotIdxToPowerVoltage(pos) : digipotIdxToMeasureVoltage(pos);
    }
    else {
        bsSetError(I2C_ERROR_Msk(channel));
    }
}

/*!
** @brief Forces the measurement range to be suitable for 4 - 20 mA for current measurement channels
*/
static void forceCurrentMeasurementRange() {
    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        if(cal.measurementType[i]) {
            setDigipotWiper(&measure_pots[i], i, measureVoltageToDigipotIdx(V_REF), false);
        }
    }
}

/*!
** @brief Gradually update the digipot wipers to their target position
*/
static void updateDigipotWipers() {
    for(int i = 0; i< NO_CALIBRATION_CHANNELS; i++) {
        digipot_t* dp = &measure_pots[i];
        if(dp->wiperTarget > dp->wiperPos) {
            setDigipotWiper(dp, i, dp->wiperPos + 1, false);
        }
        else if(dp->wiperTarget < dp->wiperPos) {
            setDigipotWiper(dp, i, dp->wiperPos - 1, false);
        }

        dp = &power_pots[i];
        if(dp->wiperTarget > dp->wiperPos) {
            setDigipotWiper(dp, i, dp->wiperPos + 1, true);
        }
        else if(dp->wiperTarget < dp->wiperPos) {
            setDigipotWiper(dp, i, dp->wiperPos - 1, true);
        }
    }
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief   Initialization function
 * @note    Should be called once before analogInputLoop()
 * @param   hadc Pointer to ADC handler
 * @param   hcrc Pointer to CRC handler
 */
void analogInputInit(ADC_HandleTypeDef *hadc, CRC_HandleTypeDef *hcrc, I2C_HandleTypeDef *_hi2c, 
                     const char *bootMsg) {
    assert_param(hadc != NULL);
    assert_param(hi2c != NULL);
    assert_param(hcrc != NULL);

    initCAProtocol(&caProto, usbRx);

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));
    calibrationInit(hcrc, &cal, sizeof(cal));
    forceCurrentMeasurementRange();

    /* Initialise basic uptime counters */
    uptime_init(hcrc, 0, NULL, bootMsg, GIT_VERSION);

    hi2c = _hi2c;

    if(0 != boardSetup(AnalogInput, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, ANALOG_INPUT_ERROR_Msk)) {
        // Don't allow initialising outputs if there is a version error
        return;
    }

    SubBoardType st;
    if(0 == getBoardInfo(NULL, &st)) {
        if(st == 1) {
            num_digipot_bits = 8U;
        }
    }
    else {
        bsSetError(BS_VERSION_ERROR_Msk);
        return;
    }

    for(int i = 0; i < NO_CALIBRATION_CHANNELS; i++) {
        (void) initDigiPots(i);
    }
    stmGpioInit(&boost_en, BOOST_EN_GPIO_Port, BOOST_EN_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(boost_en, true); // Enable boost converter
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void analogInputLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();
    ADCMonitorLoop(adcCallback);
    uptime_update();
    updateDigipotWipers();
}
