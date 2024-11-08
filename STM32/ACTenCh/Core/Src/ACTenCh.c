/*
 * ACTenCh.c
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
#include "ACTenCh.h"
#include "pcbversion.h"
#include "DS18B20.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS	        10
#define ADC_CHANNEL_BUF_SIZE	400

#define MAX_TEMPERATURE         70

#define USB_COMMS_TIMEOUT_MS    5000

/***************************************************************************************************
** PRIVATE TYPEDEFS
***************************************************************************************************/

typedef struct ActuationInfo {
    int pin; // pins 0-9 are interpreted as single ports - pin '-1' is interpreted as all
    int pwmDutyCycle;
    int timeOn; // time on is in seconds
    bool isInputValid;
} ActuationInfo;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void CAallOn(bool isOn, int duration);
static void CAportState(int port, bool state, int percent, int duration);
static void userInput(const char *input);
static void printAcTenChannelStatus();
static void updateBoardStatus();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

/* GPIO settings. */
static struct
{
    StmGpio heater;
} heaterPorts[AC_TEN_CH_NUM_PORTS];
static StmGpio fanCtrl;
static StmGpio DQ1;
static double heatSinkTemperature = 0; // Heat Sink temperature
static bool isFanForceOn = false;

static CAProtocolCtx caProto =
{
        .undefined = userInput,
        .printHeader = CAPrintHeader,
        .printStatus = printAcTenChannelStatus,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL,
        .allOn = CAallOn,
        .portState = CAportState,
};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Verbose print of the AC board status
*/
static void printAcTenChannelStatus()
{
    static char buf[600] = { 0 };
    int len = 0;

    len += snprintf(&buf[len], sizeof(buf) - len, "Fan     On: %d\r\n", stmGetGpio(fanCtrl));
    for (int i = 0; i < AC_TEN_CH_NUM_PORTS; i++)
    {
        len += snprintf(&buf[len], sizeof(buf) - len, "Port %d: On: %d, PWM percent: %d\r\n", 
                        i, stmGetGpio(heaterPorts[i].heater), getPWMPinPercent(i));
    }

    writeUSB(buf, len);
}

static void userInput(const char *input)
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
        HALundefined(input);
    }
}

static void GpioInit()
{
    const int NO_PORTS = AC_TEN_CH_NUM_PORTS;
    static GPIO_TypeDef *const pinsBlk[] = { CTRL_0_GPIO_Port, CTRL_1_GPIO_Port, CTRL_2_GPIO_Port, CTRL_3_GPIO_Port, CTRL_4_GPIO_Port,
                                             CTRL_5_GPIO_Port, CTRL_6_GPIO_Port, CTRL_7_GPIO_Port, CTRL_8_GPIO_Port, CTRL_9_GPIO_Port };
    static const uint16_t pins[] = { CTRL_0_Pin, CTRL_1_Pin, CTRL_2_Pin, CTRL_3_Pin, CTRL_4_Pin,
                                     CTRL_5_Pin, CTRL_6_Pin, CTRL_7_Pin, CTRL_8_Pin, CTRL_9_Pin };

    for (int i = 0; i < NO_PORTS; i++)
    {
        stmGpioInit(&heaterPorts[i].heater, pinsBlk[i], pins[i], STM_GPIO_OUTPUT);
        heatCtrlAdd(&heaterPorts[i].heater);
    }

    stmGpioInit(&fanCtrl, FAN_CTRL_GPIO_Port, FAN_CTRL_Pin, STM_GPIO_OUTPUT);
}

static double ADCtoCurrent(double adc_val)
{
    static float CURRENT_SCALAR = 0.013138;
    static float CURRENT_BIAS = -0.01;

    return CURRENT_SCALAR * adc_val + CURRENT_BIAS;
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
        return;
    }
    port_close_time = 0;

    /* If the version is incorrect, there is no point printing data or doing maths */
    if (bsGetStatus() & BS_VERSION_ERROR_Msk)
    {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    if (!isCalibrationDone)
    {
        for (int i = 0; i < noOfChannels; i++)
        {
            // finding the average of each channel array to subtract from the readings
            current_calibration[i] = -ADCMean(pData, i);
        }
        isCalibrationDone = true;
    }

    // Set bias for each channel.
    for (int i = 0; i < noOfChannels; i++)
    {
        // Go from channel 1 since 0 is temperature.
        ADCSetOffset(pData, current_calibration[i], i);
    }

    float heatSinkTemperature = getTemp();
    USBnprintf("%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.2f, 0x%08" PRIx32, 
                ADCtoCurrent(ADCrms(pData, 0)), ADCtoCurrent(ADCrms(pData, 1)), ADCtoCurrent(ADCrms(pData, 2)), 
                ADCtoCurrent(ADCrms(pData, 3)), ADCtoCurrent(ADCrms(pData, 4)), ADCtoCurrent(ADCrms(pData, 5)), 
                ADCtoCurrent(ADCrms(pData, 6)), ADCtoCurrent(ADCrms(pData, 7)), ADCtoCurrent(ADCrms(pData, 8)), 
                ADCtoCurrent(ADCrms(pData, 9)), heatSinkTemperature, bsGetStatus());
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
    uint8_t pwmPercent = getPWMPinPercent(port-1);
    // If heat sink has reached the maximum allowed temperature and user
    // tries to heat the system further up then disregard the input command
    if (heatSinkTemperature > MAX_TEMPERATURE && percent > pwmPercent)
        return;

    if((duration <= 0) && (percent != 0)) 
    {
        char buf[20] = {0};
        snprintf(buf, 20, "p%d on %d", port, duration);
        HALundefined(buf);
    }
    else 
    {
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
    static unsigned long previous = 0;
    // Turn on fan if temp > 55 and turn of when temp < 50.
    if (heatSinkTemperature <= MAX_TEMPERATURE)
    {
        if (heatSinkTemperature < 50 && !isFanForceOn)
        {
            stmSetGpio(fanCtrl, false);
        }
        else if (heatSinkTemperature > 55)
        {
            stmSetGpio(fanCtrl, true);
        }

        bsClearField(BS_OVER_TEMPERATURE_Msk);
    }
    else // Safety mode to avoid overheating of the board
    {
        stmSetGpio(fanCtrl, true);
        
        unsigned long now = HAL_GetTick();
        if (now - previous > 2000)  // 2000ms. Should be aligned with the control period of heater.
        {
            previous = now;
            adjustPWMDown();
        }

        bsSetError(BS_OVER_TEMPERATURE_Msk);
    }

    setBoardTemp(heatSinkTemperature);
}

/*!
** @brief Updates the global error/status object.
**
** Adds the port and fan status bits into the error field, and clears the error bit if there are no
** more errors.
*/
static void updateBoardStatus() 
{
    stmGetGpio(fanCtrl) ? bsSetField(AC_TEN_CH_PORT_x_STATUS_Msk(0)) : bsClearField(AC_TEN_CH_PORT_x_STATUS_Msk(0));
    for(int i = 0; i < AC_TEN_CH_NUM_PORTS; i++)
    {
        stmGetGpio(heaterPorts[i].heater) ? bsSetField(AC_TEN_CH_PORT_x_STATUS_Msk(i+1)) : bsClearField(AC_TEN_CH_PORT_x_STATUS_Msk(i+1));
    }

    /* Clear the error mask if there are no error bits set any more. This logic could be done when
    ** the (other) error bits are cleared, but doing here means it only needs to be done once */
    bsClearError(AC_TEN_CH_No_Error_Msk);
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
void ACTenChannelInit(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* htim, TIM_HandleTypeDef* hDS18B20tim)
{
    // Always allow for DFU also if programmed on non-matching board or PCB version.
    initCAProtocol(&caProto, usbRx);

    /* Don't initialise any outputs or act on them if the board isn't correct */
    if(boardSetup(ACTenChannel, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}) == -1)
    {
        return;
    }

    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.
    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));
    HAL_TIM_Base_Start(htim);
    GpioInit();

    DS18B20Init(hDS18B20tim, &DQ1, DQ1_GPIO_Port, DQ1_Pin);
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
void ACTenChannelLoop(const char *bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();
    ADCMonitorLoop(printCurrentArray);
    heatSinkLoop();

    // Toggle pins if needed when in pwm mode
    heaterLoop();
}
