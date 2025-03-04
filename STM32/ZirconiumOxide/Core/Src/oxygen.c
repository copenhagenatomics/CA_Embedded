/*
 * oxygen.c
 *
 *  Created on: Apr 12, 2022
 *      Author: matias
 */
#include <stdio.h>
#include <math.h>
#include <inttypes.h>
#include <string.h>

#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "StmGpio.h"
#include "oxygen.h"
#include "ZrO2Sensor.h"
#include "calibration.h"
#include "activeHeatCtrl.h"
#include "pcbversion.h"
#include "array-math.h"
#include "time32.h"
#include "uptime.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

// ADC buffer setup
#define SENSOR_CHANNELS			   2
#define ADC_CHANNELS               7	// Channel order: Sensor1, Sensor2, Current1, Current2, Voltage1, Voltage2, Vref.
#define ADC_CHANNEL_BUF_SIZE     400

#define COUNTER_FREQUENCY		  10  // 10 Hertz
#define AVG_FILTER_LEN			  50

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static ZrO2Device ZrO2Sensor[SENSOR_CHANNELS];
static bool startUpFinished = false;

static int initSensorIdx = 0;

// Instantaneous and average oxygen levels
static double avgOxygen[SENSOR_CHANNELS] = {-1.0, -1.0};
static double oxygenLevel[SENSOR_CHANNELS] = {-1.0, -1.0};

static float Rsensor[2] = {0};

// Moving average filters
moving_avg_cbuf_t maHighRange;
moving_avg_cbuf_t maLowRange;
moving_avg_cbuf_t *pMaSensors[NO_SENSOR_CHANNELS] = {&maHighRange, &maLowRange};

static double maHighArr[AVG_FILTER_LEN] = {0};
static double maLowArr[AVG_FILTER_LEN] = {0};

static TIM_HandleTypeDef* loopTimer = NULL;

/***************************************************************************************************
** PRIVATE FUNCTION PROTOTYPES
***************************************************************************************************/

static void ZrO2UserHandling(const char *input);
static void printZrO2Header();
static void printZrO2Status();
typedef void (*adcCallback)(int16_t *pData, int noOfChannels, int noOfSamples);
static void measurementLoop(int16_t *pData, int noOfChannels, int noOfSamples);
static void calibrateInterface(int noOfCalibrations, const CACalibration* calibrations);
static void calibrationRoutine(bool write);
static void enableBoard();
static void disableAndResetBoard();
static void switchSensor();
static void switchHeating();
static void updateBoardStatus();

static adcCallback adcCbFunc = measurementLoop;

static CAProtocolCtx caProto =
{
        .undefined = ZrO2UserHandling,
        .printHeader = printZrO2Header,
        .printStatus = printZrO2Status,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = calibrateInterface,
        .calibrationRW = calibrationRoutine,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Print of the ZrO2 serial information and calibration values
*/
static void printZrO2Header()
{
    CAPrintHeader();
    calibrationReadWrite(false);
}

/*!
** @brief Verbose print of the ZrO2 board status
*/
static void printZrO2Status()
{
    static char buf[600] = {0};
    int len = 0;

    uint32_t status = bsGetStatus();
    len += snprintf(&buf[len], sizeof(buf) - len, "High range sensor is: %s\r\n", (status & HIGH_RANGE_SENSOR_ON_Msk) ? "On" : "Off");
    len += snprintf(&buf[len], sizeof(buf) - len, "Low range sensor is: %s\r\n", (status & LOW_RANGE_SENSOR_ON_Msk) ? "On" : "Off");
    len += snprintf(&buf[len], sizeof(buf) - len, "Oxygen level is below measurement range: %s\r\n", (status & BELOW_RANGE_STATUS_Msk) ? "Yes" : "No");
    len += snprintf(&buf[len], sizeof(buf) - len, "High range sensor has heater error: %s\r\n", (status & HIGH_RANGE_HEATER_ERROR_Msk) ? "Yes" : "No");
    len += snprintf(&buf[len], sizeof(buf) - len, "Low range sensor has heater error: %s\r\n", (status & LOW_RANGE_HEATER_ERROR_Msk) ? "Yes" : "No");

    Uptime ut = getUptime();
    len += snprintf(&buf[len], sizeof(buf) - len, "Board total on time: %u min.\r\n", ut.board_uptime);
    len += snprintf(&buf[len], sizeof(buf) - len, "High range sensor total on time: %u min.\r\n", ut.high_sensor_uptime);
    len += snprintf(&buf[len], sizeof(buf) - len, "High range heater total on time: %u min.\r\n", ut.high_heater_uptime);
    len += snprintf(&buf[len], sizeof(buf) - len, "Low range sensor total on time: %u min.\r\n", ut.low_sensor_uptime);
    len += snprintf(&buf[len], sizeof(buf) - len, "Low range heater total on time: %u min.\r\n", ut.low_heater_uptime);

    writeUSB(buf, len);
}

/*!
** @brief Store and print calibration data
*/
static void calibrationRoutine(bool write)
{
    updateCalibrationInfo();
    calibrationReadWrite(write);
}

/*!
** @brief Takes in calibration data and starts calibration method
** @note Called via the calibration interface command defined in CAProtocol.c
*/
static void calibrateInterface(int noOfCalibrations, const CACalibration* calibrations)
{
    calibrateSensor(noOfCalibrations, calibrations);
}

/*!
** @brief Enable heaters to return to normal board operation
** @note Called through serial interface via the "zro2 on" command
*/
static void enableBoard()
{
    enableHeaters();
}

/*!
** @brief Disable sensors and heaters and set board into idle mode
** @note Called through serial interface via the "zro2 off" command
*/
static void disableAndResetBoard()
{
    startUpFinished = 0;
    for (int i = 0; i < NO_SENSOR_CHANNELS; i++)
    {
        // Log which sensor to turn on when the board is re-heated
        initSensorIdx = (isSensorOn(i)) ? i : initSensorIdx;
        turnOffSensor(&ZrO2Sensor[i], i);
    }
    disableAndResetHeaters();
}

/*!
** @brief Board specific serial commands
*/
static void ZrO2UserHandling(const char *input)
{
    int port;
    int gain;
    float bias, scalar, quadrant;
    float rCold;
    float factor;

    if (sscanf(input, "turnOn p%d", &port) == 1)
    {
        // Force turn on sensor. Used to ensure it is working as expected.
        if (!isSensorHeated(port-1))
        {
            USBnprintf("Sensor must be heated to be turned on");
            return;
        }
        turnOnSensor(&ZrO2Sensor[port-1], port-1);
    }
    else if (sscanf(input, "turnOff p%d", &port) == 1)
    {
        // Force turn off sensor.
        turnOffSensor(&ZrO2Sensor[port-1], port-1);
    }
    else if (sscanf(input, "rcold p%d %f", &port, &rCold) == 2)
    {
        updateRCold(port - 1, rCold);
    }
    else if (sscanf(input, "tempFactor %f", &factor) == 1)
    {
        updateTempFactor(factor);
    }
    else if (sscanf(input, "calibrate p%d %d,%f,%f,%f", &port, &gain, &bias, &scalar, &quadrant) == 5)
    {
        calibrateManual(&ZrO2Sensor[port-1], port-1, gain, bias, scalar, quadrant);
    }
    else if (strncmp(input, "zro2 on", 7) == 0)
    {
        enableBoard();
    }
    else if (strncmp(input, "zro2 off", 8) == 0)
    {
        disableAndResetBoard();
    }
    else
    {
        HALundefined(input);
    }
}

/*!
** @brief ADC callback function that controls heaters and measures all outputs
*/
static void measurementLoop(int16_t *pData, int noOfChannels, int noOfSamples)
{
    static int hasReset = 0;

    // Regulate heating elements to operate at a steady temperature.
    activeHeatControl(pData, noOfChannels, noOfSamples);

    if (!isUsbPortOpen())
    {
        // If the USB port is not open, switch to idle mode
        if (!hasReset)
        {
            disableAndResetBoard();
            hasReset = 1;
        }
        return;
    }

    if (hasReset)
    {
        enableBoard();
        hasReset = 0;
    }

    if(BS_VERSION_ERROR_Msk & bsGetStatus()) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    for (int i = 0; i<SENSOR_CHANNELS; i++)
    {
        if (!isSensorHeated(i) || !startUpFinished || !isSensorOn(i))
        {
            avgOxygen[i] = -1.0;
            continue;
        }

        // Check whether the oxygen level is within the operating range of
        // a given sensor. If not the sensor should be shut off.
        if (ZrO2Sensor[i].state != WITHIN_RANGE && (HAL_GetTick() - ZrO2Sensor[i].offTimer >= STABILIZATION_PERIOD) && ZrO2Sensor[!i].hasStabilised)
        {
            turnOffSensor(&ZrO2Sensor[i], i);
            avgOxygen[i] = -1.0;
            continue;
        }

        // Current oxygen measurement
        double adcMean = ADCMean(pData, i);
        oxygenLevel[i] = ADCtoOxygen(&ZrO2Sensor[i], adcMean);

        // Only start printing values once the sensings have stabilized
        if (!ZrO2Sensor[i].hasStabilised)
        {
            // Add samples to moving averaging filter while stabilising
            // The stabilisation time is 20 seconds and the mvgAvg filter
            // is 5 seconds so it's fairly stabilised when outputting to
            // avgOxygen[i]
            maMean(pMaSensors[i], oxygenLevel[i]);
        }
        else
        {
            // Get average oxygen level
            avgOxygen[i] = maMean(pMaSensors[i], oxygenLevel[i]);
        }
        updateSensorState(&ZrO2Sensor[i], avgOxygen[i], adcMean);
    }
    double combinedOxygen = computeCombinedOxygen(&ZrO2Sensor[0], &ZrO2Sensor[1], avgOxygen[0], avgOxygen[1]);

    getRCurrent(&Rsensor[0], &Rsensor[1]);
    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, 0x%08" PRIx32, avgOxygen[0], avgOxygen[1], combinedOxygen, Rsensor[0], Rsensor[1], bsGetStatus());

    // Calibration routine
    for (int i = 0; i < SENSOR_CHANNELS; i++)
    {
        if (initiateCalibration(i))
            calibrateToTarget(&ZrO2Sensor[i], i, oxygenLevel[i]);
    }
}

/*!
** @brief Function that determines whether to switch on/off sensors depending on the state of the system
** @note  Called from the main timer interrupt HAL_TIM_PeriodElapsedCallback with a frequency of 10 Hz
*/
static void switchSensor()
{
    static int transitionCount[SENSOR_CHANNELS] = {0};
    static uint32_t lastOnTime[SENSOR_CHANNELS] = {0};
    static int count = 0;
    static int scheduleTurnOn[SENSOR_CHANNELS] = {0};
    int isSensorsOn[2] = {isSensorOn(0), isSensorOn(1)};

    // If both sensors have switched off and low range has not moved into below range
    // then switch on high range sensor to enter default state. This should never occur,
    // but noise in measurements makes this necessary.
    if (!isSensorsOn[0] && !isSensorsOn[1] && ZrO2Sensor[1].state != BELOW_RANGE)
        scheduleTurnOn[0] = 1;

    uint32_t now = HAL_GetTick();
    for (int idx = 0; idx < SENSOR_CHANNELS; idx++)
    {
        // If a sensor has been in the transitional state for more than 5 seconds,
        // we periodically switch on the other sensor every 15 minutes as long as it's not already on.
        // This is done since the sensors become somewhat unreliable in their transitional range.
        // If a sensor moves fully outside of its range we turn the other sensor on immediately.
        transitionCount[idx] = (ZrO2Sensor[!idx].isInTransition) ? transitionCount[idx] + 1 : 0;
        if (transitionCount[idx] >= COUNTER_FREQUENCY*5 && 
            !isSensorsOn[idx] &&
            (((now - lastOnTime[idx]) > SENSOR_CHECK_INTERVAL) || ZrO2Sensor[!idx].state != WITHIN_RANGE))
        {
            lastOnTime[idx] = now;
            scheduleTurnOn[idx] = 1;
        }
    }


    count = (ZrO2Sensor[1].state == BELOW_RANGE) ? count + 1 : 0;
    // Turn off low range sensor when it has been below range for more than 5 seconds. 
    if (ZrO2Sensor[1].state == BELOW_RANGE && isSensorsOn[1] && count >= COUNTER_FREQUENCY*5)
        turnOffSensor(&ZrO2Sensor[1], 1);

    // Turn on low range sensor periodically to see whether the oxygen level has risen.
    if (ZrO2Sensor[1].state == BELOW_RANGE && count >= COUNTER_FREQUENCY*500)
        scheduleTurnOn[1] = 1;

    // If low range is within range and not in transition turn off high range if not already turned off.
    if (isSensorsOn[1] && !ZrO2Sensor[1].isInTransition && ZrO2Sensor[1].hasStabilised)
        turnOffSensor(&ZrO2Sensor[0], 0);

    // Turn on sensor when scheduled and the sensor is heated
    for (int i = 0; i<SENSOR_CHANNELS; i++)
    {
        if (isSensorHeated(i) && scheduleTurnOn[i])
        {
            turnOnSensor(&ZrO2Sensor[i], i);
            scheduleTurnOn[i] = 0;
        }
    }
}

/*!
** @brief Function that determines whether to switch on/off heaters depending on the state of the system
** @note  Called from the main timer interrupt HAL_TIM_PeriodElapsedCallback with a frequency of 10 Hz
*/
static void switchHeating()
{
    static int count = 0;
    // If high range is stable and below 2% turn on low range heater
    if (isSensorOn(0) && ZrO2Sensor[0].hasStabilised)
    {
        (avgOxygen[0] <= LOW_HEATER_TRANSITION_LEVEL) ? turnOnHeating(1) : turnOffHeating(1);
        return;
    }

    // If low range is stable and above 800ppm turn on high range heater
    if (isSensorOn(1) && ZrO2Sensor[1].hasStabilised)
    {
        (avgOxygen[1] >= HIGH_HEATER_TRANSITION_LEVEL) ? turnOnHeating(0) : turnOffHeating(0);
        return;
    }

    // If low range is below range then both sensors are shut off. Turn on low range sensor
    // periodically to see whether the oxygen level has risen.
    count = (ZrO2Sensor[1].state == BELOW_RANGE) ? count + 1 : 0;
    // Heat up sensor before switching on again
    if (ZrO2Sensor[1].state == BELOW_RANGE && count >= COUNTER_FREQUENCY*400)
    {
        turnOnHeating(1);
        return;
    }

    if (ZrO2Sensor[1].state == BELOW_RANGE && !isSensorOn(1))
    {
        turnOffHeating(1);
    }
}

/*!
** @brief Updates the board status code which is printed in the measurementLoop(...) function
*/
static void updateBoardStatus()
{
    if(BS_VERSION_ERROR_Msk & bsGetStatus()) {
        return;
    }

    // Status bits for whether high / low range sensors are currently on
    void (*setter)(uint32_t) = (isSensorOn(0)) ? bsSetField : bsClearField;
    setter(HIGH_RANGE_SENSOR_ON_Msk);

    setter = (isSensorOn(1)) ? bsSetField : bsClearField;
    setter(LOW_RANGE_SENSOR_ON_Msk);

    /* When the low range sensor is below range (<10ppm) this field is set
    ** This helps differentiate between the low range sensor simply being turned off
    ** or whether it is being turned on periodically */
    setter = (ZrO2Sensor[1].state == BELOW_RANGE) ? bsSetField : bsClearField;
    setter(BELOW_RANGE_STATUS_Msk);

    /* Both heater resistances can be at max 3.45Ohm * 2.8 = 9.55 Ohm otherwise
    ** there is an indication of a heater failing */
    setter = (Rsensor[0] > 10) ? bsSetError : bsClearField;
    setter(HIGH_RANGE_HEATER_ERROR_Msk);

    setter = (Rsensor[1] > 10) ? bsSetError : bsClearField;
    setter(LOW_RANGE_HEATER_ERROR_Msk);

    // Clear errors if no error masks are set
    bsClearError(ZRO2_No_Error_Msk);
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Timer callback function
** @note  Turns on high range sensor when initial heating is complete and is swithes heaters and sensor
**        afterwards. It is called with a frequency of 10 Hz.
*/
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim != loopTimer) return;

    if (isInitHeatingCompleted() && !startUpFinished)
    {
        turnOnSensor(&ZrO2Sensor[initSensorIdx], initSensorIdx);
        startUpFinished = true;
        return;
    }
    
    if (startUpFinished)
    {
        switchSensor();
        switchHeating();
    }
}

/*!
** @brief Board initialization function
*/
void oxygenInit(ADC_HandleTypeDef* hadc_, TIM_HandleTypeDef* adcTimer, TIM_HandleTypeDef* loopTimer_, CRC_HandleTypeDef* hcrc)
{
    initCAProtocol(&caProto, usbRx); // Always allow for DFU even if board type does not match.

    HAL_TIM_Base_Start(adcTimer);
    HAL_TIM_Base_Start_IT(loopTimer_);

    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];
    ADCMonitorInit(hadc_, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));
    loopTimer = loopTimer_;

    /* Don't initialise any outputs or act on them if the board isn't correct */
    if(boardSetup(ZrO2Oxygen, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}) == -1)
    {
        return;
    }

    maInit(&maHighRange, maHighArr, AVG_FILTER_LEN);
    maInit(&maLowRange, maLowArr, AVG_FILTER_LEN);

    ZrO2SensorInit(&ZrO2Sensor[0], &ZrO2Sensor[1]);
    initCalibration(&ZrO2Sensor[0], &ZrO2Sensor[1], hcrc);
    heatControlInit(&ZrO2Sensor[0], &ZrO2Sensor[1]);
    initUptime(hcrc);
}

/*!
** @brief Main program loop
*/
void oxygenLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(adcCbFunc);
    updateBoardStatus();
    updateUptime();
}
