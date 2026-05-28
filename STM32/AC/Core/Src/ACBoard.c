/*
 * ACBoard.c
 */

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <float.h>
#include <inttypes.h>

#include "main.h"
#include "HeatCtrl.h"
#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "ACBoard.h"
#include "faultHandlers.h"
#include "pcbversion.h"
#include "flashHandler.h"
#include "CAProtocolACDC.h"
#include "array-math.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS                8   // 4 current + 4 temperature
#define ADC_CHANNEL_BUF_SIZE      400
#define NUM_CURRENT_CHANNELS        4
#define NUM_TEMP_CHANNELS           4

#define MAX_TEMPERATURE            70
#define MAX_ON_TIME_REQUEST        10 // seconds

#define USB_COMMS_TIMEOUT_MS     5000

#define EFUSE_DEFAULT_CURRENT_LIMIT_A  10.0f
#define EFUSE_MA_WINDOW                10   // samples per PWM period (10 Hz × 1 s)

/***************************************************************************************************
** PRIVATE TYPEDEFS
***************************************************************************************************/

typedef struct ActuationInfo {
    int pin; // pins 0-3 are interpreted as single ports - pin '-1' is interpreted as all
    int pwmDutyCycle;
    int timeOn; // time on is in seconds
    bool isInputValid;
} ActuationInfo;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void CAallOn(bool isOn, int duration);
static void CAportState(int port, bool state, int percent, int duration);
static void ACInputHandler(const char *input);
static void printAcStatus();
static void updateBoardStatus();
static void printAcHeader();
static void printAcStatusDef();
static void computeHeatSinkTemperatures(int16_t *pData);
static void printCurrentArray(int16_t *pData, int noOfChannels, int noOfSamples);
static void GpioInit();
static double ADCtoCurrent(double adc_val);
static double ADCtoTemperature(double adc_val);
static void actuatePins(ActuationInfo actuationInfo);
static void heatSinkLoop();
static void ACcalibration(int noOfCalibrations, const CACalibration* calibrations);
static void ACcalibrationRW(bool write);
static void efuseLoop(const double *currents);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

/* GPIO settings. */
static struct
{
    StmGpio heater;
    StmGpio button;
} heaterPorts[AC_BOARD_NUM_PORTS];
static StmGpio fanCtrl;
static StmGpio powerStatus;
static double heatSinkTemperatures[NUM_TEMP_CHANNELS] = {0};
static double heatSinkMaxTemp = 0;
static float isMainsConnected = 0;
static bool isFanForceOn = false;
static float *efuseCurrentLimits = NULL;
static moving_avg_cbuf_t efuseMaFilter[AC_BOARD_NUM_PORTS];
static double efuseMaBuffer[AC_BOARD_NUM_PORTS][EFUSE_MA_WINDOW];

static ACDCProtocolCtx acProto =
{
        .allOn = CAallOn,
        .portState = CAportState
};

static CAProtocolCtx caProto =
{
        .undefined = ACInputHandler,
        .printHeader = printAcHeader,
        .printStatus = printAcStatus,
        .printStatusDef = printAcStatusDef,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = ACcalibration,
        .calibrationRW = ACcalibrationRW,
        .logging = NULL,
        .otpRead = CAotpRead
};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Updates calibration memory with per-channel e-fuse current limits.
**        Command format: "calibration p<N>,<limit_amps>,0,0"
*/
static void ACcalibration(int noOfCalibrations, const CACalibration* calibrations) {
    for (int i = 0; i < noOfCalibrations; i++) {
        int port = calibrations[i].port;
        if (port >= 1 && port <= AC_BOARD_NUM_PORTS && calibrations[i].alpha > 0) {
            efuseCurrentLimits[port - 1] = (float)calibrations[i].alpha;
        }
    }
}

/*!
** @brief Reads or writes e-fuse current limits to/from flash.
*/
static void ACcalibrationRW(bool write) {
    if (write) {
        fhSaveDeposit();
    }
    else {
        char buf[300];
        int len = 0;

        CA_SNPRINTF(buf, len, "Calibration: CAL");
        for (int ch = 0; ch < AC_BOARD_NUM_PORTS; ch++) {
            CA_SNPRINTF(buf, len, " %d,%.2f,0,0", ch + 1, efuseCurrentLimits[ch]);
        }
        CA_SNPRINTF(buf, len, "\r\n");
        writeUSB(buf, len);
    }
}

/*!
** @brief Checks per-channel average current and trips the e-fuse if exceeded.
**
** Uses a sliding window average over the last EFUSE_MA_WINDOW callbacks (= 1 PWM period),
** checked on every ADC callback for the fastest possible response.
*/
static void efuseLoop(const double* currents) {
    for (int i = 0; i < AC_BOARD_NUM_PORTS; i++) {
        double avg = maMean(&efuseMaFilter[i], currents[i]);
        if (avg > efuseCurrentLimits[i]) {
            turnOffPin(i);
            bsSetError(AC_EFUSE_OVERCURRENT_Msk(i + 1));
        }
    }
}

/*!
** @brief Printout of the general board info, e.g. serial number, sw version, etc...
*/
static void printAcHeader() {
    CAPrintHeader();
}

/*!
** @brief Verbose print of the AC board status
*/
static void printAcStatus() {
    static char buf[600] = {0};
    int len              = 0;

    CA_SNPRINTF(buf, len, "Fan     On: %d\r\n", stmGetGpio(fanCtrl));
    for (int i = 0; i < AC_BOARD_NUM_PORTS; i++) {
        CA_SNPRINTF(buf, len, "Port %d: On: %d, PWM percent: %d\r\n", i,
                    stmGetGpio(heaterPorts[i].heater), getPWMPinPercent(i));
    }

    CA_SNPRINTF(buf, len, "Power   On: %d\r\n", (int)round(isMainsConnected));

    writeUSB(buf, len);
}

/*!
 * @brief Definition of status definition information when the 'StatusDef' command is received
*/
static void printAcStatusDef() {
    static char buf[600] = {0};
    int len              = 0;
    CA_SNPRINTF(buf, len, "0x%08lx,Mains not-connected error\r\n", AC_POWER_ERROR_Msk);
    CA_SNPRINTF(buf, len, "0x%08lx,Port 4 switching state\r\n", AC_BOARD_PORT_x_STATUS_Msk(4));
    CA_SNPRINTF(buf, len, "0x%08lx,Port 3 switching state\r\n", AC_BOARD_PORT_x_STATUS_Msk(3));
    CA_SNPRINTF(buf, len, "0x%08lx,Port 2 switching state\r\n", AC_BOARD_PORT_x_STATUS_Msk(2));
    CA_SNPRINTF(buf, len, "0x%08lx,Port 1 switching state\r\n", AC_BOARD_PORT_x_STATUS_Msk(1));
    CA_SNPRINTF(buf, len, "0x%08lx,Fan state\r\n", AC_BOARD_PORT_x_STATUS_Msk(0));
    for (int i = 1; i <= AC_BOARD_NUM_PORTS; i++) {
        CA_SNPRINTF(buf, len, "0x%08lx,Port %d e-fuse overcurrent error\r\n",
                    AC_EFUSE_OVERCURRENT_Msk(i), i);
    }

    writeUSB(buf, len);
}

static void ACInputHandler(const char *input)
{
    if (strncmp(input, "fan on", 6) == 0)
    {
        isFanForceOn = true;
        stmSetGpio(fanCtrl, true);
    }
    else if (strncmp(input, "fan off", 7) == 0)
    {
        isFanForceOn = false;
        stmSetGpio(fanCtrl, false);
    }
    else 
    {
        ACDCInputHandler(&acProto, input);
    }
}

static void GpioInit()
{
    const int noPorts = AC_BOARD_NUM_PORTS;
    static GPIO_TypeDef *const pinsBlk[] = { ctrl1_GPIO_Port, ctrl2_GPIO_Port, ctrl3_GPIO_Port, ctrl4_GPIO_Port };
    static const uint16_t pins[] = { ctrl1_Pin, ctrl2_Pin, ctrl3_Pin, ctrl4_Pin };

    for (int i = 0; i < noPorts; i++)
    {
        stmGpioInit(&heaterPorts[i].heater, pinsBlk[i], pins[i], STM_GPIO_OUTPUT);
        heatCtrlAdd(&heaterPorts[i].heater, &heaterPorts[i].button);
    }

    stmGpioInit(&fanCtrl, fanctrl_GPIO_Port, fanctrl_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&powerStatus, powerStatus_GPIO_Port, powerStatus_Pin, STM_GPIO_INPUT);
}

static double ADCtoCurrent(double adc_val)
{
    // TODO: change method for calibration?
    static float current_scalar = 0.013138;
    static float current_bias = -0.01; //-0.058;

    return current_scalar * adc_val + current_bias;
}

static double ADCtoTemperature(double adc_val)
{
    static const float TEMP_SCALAR = 0.0806;
    static const float TEMP_BIAS = -50.0;

    return TEMP_SCALAR * adc_val + TEMP_BIAS;
}

static void computeHeatSinkTemperatures(int16_t *pData)
{
    double maxTemp = DBL_MIN;
    for (int i = 0; i < NUM_TEMP_CHANNELS; i++)
    {
        heatSinkTemperatures[i] = ADCtoTemperature(ADCMean(pData, i+NUM_CURRENT_CHANNELS));
        if (heatSinkTemperatures[i] > maxTemp)
        {
            maxTemp = heatSinkTemperatures[i];
        }
    }
    heatSinkMaxTemp = maxTemp;
}

static void printCurrentArray(int16_t *pData, int noOfChannels, int noOfSamples)
{
    // Make calibration static since this should be done only once.
    static bool isCalibrationDone = false;
    static int16_t current_calibration[ADC_CHANNELS];
    static uint32_t port_close_time = 0;

    /* If the USB port is not open, no messages should be printed. Also if the USB port has been 
    ** closed for more than a timeout, everything should be turned off as a safety measure */
    if (!isUsbPortOpen()) 
    {
        if (port_close_time == 0)
        {
            port_close_time = HAL_GetTick();
        }
        else if((HAL_GetTick() - port_close_time) >= USB_COMMS_TIMEOUT_MS)
        {
            allOff();
        }
        computeHeatSinkTemperatures(pData);
        return;
    }
    port_close_time = 0;

    /* If the version is incorrect, there is no point printing data or doing maths */
    if (bsGetStatus() & BS_VERSION_ERROR_Msk)
    {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    if (!isCalibrationDone)
    {
        for (int i = 0; i < NUM_CURRENT_CHANNELS; i++)
        {
            // finding the average of each channel array to subtract from the readings
            current_calibration[i] = -ADCMean(pData, i);
        }
        isCalibrationDone = true;
    }

    // Set bias for each current channel.
    for (int i = 0; i < NUM_CURRENT_CHANNELS; i++)
    {
        ADCSetOffset(pData, current_calibration[i], i);
    }

    computeHeatSinkTemperatures(pData);

    double currents[NUM_CURRENT_CHANNELS];
    for (int i = 0; i < NUM_CURRENT_CHANNELS; i++) {
        currents[i] = ADCtoCurrent(ADCrms(pData, i));
    }

    efuseLoop(currents);

    USBnprintf("%.4f, %.4f, %.4f, %.4f, %.2f, %.2f, %.2f, %.2f, 0x%08" PRIx32 "\r\n", 
               currents[0], currents[1], currents[2], currents[3], 
               heatSinkTemperatures[0], heatSinkTemperatures[1], 
               heatSinkTemperatures[2], heatSinkTemperatures[3],
               bsGetStatus());
}

/*!
** @brief Calls appropriate backend function based on inputs
**
** Depending on the inputs (which are received from communication link), chooses the appropriate 
** backend function and calls it.
*/
static void actuatePins(ActuationInfo actuationInfo)
{
    /* All pins functions */
    if (actuationInfo.pin == -1)
    {
        if (actuationInfo.pwmDutyCycle == 0)
        {
            // all off (pin == -1 means all pins)
            allOff();
        }
        else if (actuationInfo.pwmDutyCycle == 100)
        {
            // all on (pin == -1 means all pins)
            allOn(actuationInfo.timeOn);
        }
        /* It doesn't really make sense to allow setting all pins to the same PWM */
    } 
    else
    {
        if (actuationInfo.pwmDutyCycle == 0)
        {
            // pX off
            turnOffPin(actuationInfo.pin);
        }
        else if (actuationInfo.pwmDutyCycle == 100)
        {
            // pX on YY
            turnOnPin(actuationInfo.pin, actuationInfo.timeOn);
        }
        else
        {
            // pX on ZZZ%
            setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
        }
    }
}

static void CAallOn(bool isOn, int duration)
{
    if (isOn)
    {
        if(duration <= 0) 
        {
            char buf[20] = {0};
            snprintf(buf, 20, "all on %d", duration);
            HALundefined(buf);
        }
        else
        {
            bsClearField(AC_EFUSE_OVERCURRENT_ALL_Msk);
            allOn(1000*duration);
        }
    }
    else
    {
        allOff();
    }
}
static void CAportState(int port, bool state, int percent, int duration)
{
    // If heat sink has reached the maximum allowed temperature and user
    // tries to heat the system further up then disregard the input command
    if (heatSinkMaxTemp > MAX_TEMPERATURE)
        return;

    if((duration <= 0) && (percent != 0)) 
    {
        char buf[20] = {0};
        snprintf(buf, 20, "p%d on %d", port, duration);
        HALundefined(buf);
    }
    else
    {
        bsClearField(AC_EFUSE_OVERCURRENT_Msk(port));

        if (duration > MAX_ON_TIME_REQUEST)
        {
            duration = MAX_ON_TIME_REQUEST;
            bsSetField(AC_LIMIT_ON_TIME_STATUS_Msk);
        }
        else
        {
            bsClearField(AC_LIMIT_ON_TIME_STATUS_Msk);
        }
        actuatePins((ActuationInfo) { port - 1, percent, 1000*duration, true });
    }
}

/*!
** @brief Loop for controlling board temperature.
**
** Loops keeping the board cool using the attached fan. If the board gets too hot, starts to reduce
** the power on all ports by reducing the PWM
*/
static void heatSinkLoop()
{
    // Turn on fan if temp > 55 and turn of when temp < 50.
    if (heatSinkMaxTemp <= MAX_TEMPERATURE)
    {
        if (heatSinkMaxTemp < 50 && !isFanForceOn)
        {
            stmSetGpio(fanCtrl, false);
        }
        else if (heatSinkMaxTemp > 55)
        {
            stmSetGpio(fanCtrl, true);
        }

        bsClearField(BS_OVER_TEMPERATURE_Msk);
    }
    else 
    {
        /* Board is running above max temperature 
        ** NOTE: As the max on time per request is 10 seconds, it is deemed safe
        **       to let the board run until the next timeout (<=10 sec) occurs.
        **       While the board is above max temperature it ignores any new requests. */
        stmSetGpio(fanCtrl, true);
        bsSetError(BS_OVER_TEMPERATURE_Msk);
    }

    setBoardTemp(heatSinkMaxTemp);
}

/*!
** @brief Updates the global error/status object.
**
** Adds the port and fan status bits into the error field, and clears the error bit if there are no
** more errors.
*/
static void updateBoardStatus() 
{
    static int const FILTER_LEN = 1000;

    stmGetGpio(fanCtrl) ? bsSetField(AC_BOARD_PORT_x_STATUS_Msk(0)) : bsClearField(AC_BOARD_PORT_x_STATUS_Msk(0));
    for(int i = 0; i < AC_BOARD_NUM_PORTS; i++)
    {
        stmGetGpio(heaterPorts[i].heater) ? bsSetField(AC_BOARD_PORT_x_STATUS_Msk(i+1)) : bsClearField(AC_BOARD_PORT_x_STATUS_Msk(i+1));
    }

    /* Heavily averaged signal of the last 1000 samples to smooth out large dips
    ** The gpio input is not supposed to change value generally so should be fine to use a large filter
    ** Note: When power is toggled the status code changes within one print cycle */
    isMainsConnected += (stmGetGpio(powerStatus) - isMainsConnected)/FILTER_LEN;
    (isMainsConnected >= 0.5) ? bsClearField(AC_POWER_ERROR_Msk) : bsSetError(AC_POWER_ERROR_Msk);

    /* Clear the error mask if there are no error bits set any more. This logic could be done when
    ** the (other) error bits are cleared, but doing here means it only needs to be done once */
    bsClearError(AC_BOARD_No_Error_Msk);
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Setup function called at startup
**
** Checks the hardware matches this FW version, starts the USB communication and starts the ADC. 
** Printing is synchronised with ADC, so it must be started in order to print anything over the USB
** link
*/
void ACBoardInit(ADC_HandleTypeDef* hadc)
{
    // Pin out has changed from PCB V6.4 - older versions need other software.
    boardSetup(AC_Board, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, AC_BOARD_No_Error_Msk);

    // Always allow for DFU also if programmed on non-matching board or PCB version.
    initCAProtocol(&caProto, usbRx);

    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));
    GpioInit();

    /* Setup flash handling */
    fhLoadDeposit();
    setLocalFaultInfo(fhGetFaultInfo());

    /* Initialise e-fuse current limits; use default for any channel not yet written to flash */
    efuseCurrentLimits = fhGetCurrentLimits();
    for (int i = 0; i < AC_BOARD_NUM_PORTS; i++) {
        if (!isfinite(efuseCurrentLimits[i]) || efuseCurrentLimits[i] <= 0) {
            efuseCurrentLimits[i] = EFUSE_DEFAULT_CURRENT_LIMIT_A;
        }
        maInit(&efuseMaFilter[i], efuseMaBuffer[i], EFUSE_MA_WINDOW);
    }
}

/*!
** @brief Loop function called repeatedly throughout
** 
** * Responds to user input
** * Checks for ADC buffer switches (the ADC sample rate is synchronised with USB print rate - the 
**   USB print rate should be 10 Hz, so every 400 ADC samples the buffer switches).
** * Runs the closed loop control system for board temperature and PWMs the heaters as per user 
**   input
*/
void ACBoardLoop(const char *bootMsg)
{
    if(CAhandleUserInputs(&caProto, bootMsg)) {
        /* If a serious fault that required a reset occurs, print the stack trace immediately upon boot */
        if(printFaultInfo()) {
            clearFaultInfo();
            fhSaveDeposit();
        }
    };
    updateBoardStatus();
    ADCMonitorLoop(printCurrentArray);
    heatSinkLoop();

    // Toggle pins if needed when in pwm mode
    heaterLoop();
}
