/*
 * ACBoard.c
 */

#include <math.h>
#include "string.h"
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <float.h>
#include "usb_cdc_fops.h"
#include "HeatCtrl.h"
#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "HAL_otp.h"
#include "StmGpio.h"

#define ADC_CHANNELS	5
#define ADC_CHANNEL_BUF_SIZE	400

#define MAX_TEMPERATURE 70

/* GPIO settings. */
static struct
{
    StmGpio heater;
    StmGpio button;
} heaterPorts[4];
static StmGpio fanCtrl;
static double heatSinkTemperature = 0; // Heat Sink temperature
static bool isFanForceOn = false;

// Forward declare functions.
static void CAallOn(bool isOn);
static void CAportState(int port, bool state, int percent, int duration);
static void userInput(const char *input);
static CAProtocolCtx caProto =
{
        .undefined = userInput,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL,
        .allOn = CAallOn,
        .portState = CAportState,
};

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
}

static void GpioInit()
{
    const int noPorts = 4;
    static GPIO_TypeDef *const pinsBlk[] = { ctrl1_GPIO_Port, ctrl2_GPIO_Port, ctrl3_GPIO_Port, ctrl4_GPIO_Port };
    static const uint16_t pins[] = { ctrl1_Pin, ctrl2_Pin, ctrl3_Pin, ctrl4_Pin };

    for (int i = 0; i < noPorts; i++)
    {
        stmGpioInit(&heaterPorts[i].heater, pinsBlk[i], pins[i], STM_GPIO_OUTPUT);
        heatCtrlAdd(&heaterPorts[i].heater, &heaterPorts[i].button);
    }

    stmGpioInit(&fanCtrl, powerCut_GPIO_Port, powerCut_Pin, STM_GPIO_OUTPUT);
}

static double ADCtoCurrent(double adc_val)
{
    // TODO: change method for calibration?
    static float current_scalar = 0.0125;
    static float current_bias = -0.1187; //-0.058;

    return current_scalar * adc_val + current_bias;
}

static double ADCtoTemperature(double adc_val)
{
    // TODO: change method for calibration?
    static float temp_scalar = 0.0806;

    return temp_scalar * adc_val;
}

static void printCurrentArray(int16_t *pData, int noOfChannels, int noOfSamples)
{
    // Make calibration static since this should be done only once.
    static bool isCalibrationDone = false;
    static int16_t current_calibration[ADC_CHANNELS];

    if (!isComPortOpen()) return;

    if (!isCalibrationDone)
    {
        // Go from channel 1 since 0 is temperature.
        for (int i = 1; i < noOfChannels; i++)
        {
            // finding the average of each channel array to subtract from the readings
            current_calibration[i] = -ADCMean(pData, i);
        }
        isCalibrationDone = true;
    }

    // Set bias for each channel.
    for (int i = 1; i < noOfChannels; i++)
    {
        // Go from channel 1 since 0 is temperature.
        ADCSetOffset(pData, current_calibration[i], i);
    }

    heatSinkTemperature = ADCtoTemperature(ADCMean(pData, 0));
    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f", ADCtoCurrent(ADCrms(pData, 1)),
            ADCtoCurrent(ADCrms(pData, 2)), ADCtoCurrent(ADCrms(pData, 3)),
            ADCtoCurrent(ADCrms(pData, 4)),
            heatSinkTemperature);
}

typedef struct ActuationInfo {
    int pin; // pins 0-3 are interpreted as single ports - pin '-1' is interpreted as all
    int pwmDutyCycle;
    int timeOn; // time on is in seconds - timeOn '-1' is interpreted as indefinitely
    bool isInputValid;
} ActuationInfo;
static void actuatePins(ActuationInfo actuationInfo)
{
    if (actuationInfo.pin == -1 && actuationInfo.pwmDutyCycle == 0)
    {
        // all off (pin == -1 means all pins)
        allOff();
    }
    else if (actuationInfo.pin == -1 && actuationInfo.pwmDutyCycle == 100)
    {
        // all on (pin == -1 means all pins)
        allOn();
    }
    else if (actuationInfo.timeOn == -1 && (actuationInfo.pwmDutyCycle == 100 || actuationInfo.pwmDutyCycle == 0))
    {
        // pX on or pX off (timeOn == -1 means indefinite)
        if (actuationInfo.pwmDutyCycle == 0)
        {
            turnOffPin(actuationInfo.pin);
        }
        else
        {
            turnOnPin(actuationInfo.pin);
        }
    }
    else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle == 100)
    {
        // pX on YY
        turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
    }
    else if (actuationInfo.timeOn == -1 && actuationInfo.pwmDutyCycle != 0 && actuationInfo.pwmDutyCycle != 100)
    {
        // pX on ZZZ%
        setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
    }
    else if (actuationInfo.timeOn != -1 && actuationInfo.pwmDutyCycle != 100)
    {
        // pX on YY ZZZ%
        setPWMPin(actuationInfo.pin, actuationInfo.pwmDutyCycle, actuationInfo.timeOn);
    }
}

static void CAallOn(bool isOn)
{
    (isOn) ? allOn() : allOff();
}
static void CAportState(int port, bool state, int percent, int duration)
{
	uint8_t pwmPercent = getPWMPinPercent(port-1);
	// If heat sink has reached the maximum allowed temperature and user
	// tries to heat the system further up then disregard the input command
	if (heatSinkTemperature > MAX_TEMPERATURE && percent > pwmPercent)
		return;

    actuatePins((ActuationInfo) { port - 1, percent, 1000*duration, true });
}

static void heatSinkLoop()
{
	static unsigned long previous = 0;
    // Turn on fan if temp > 55 and turn of when temp < 50.
    if (heatSinkTemperature < 50 && !isFanForceOn)
    {
		stmSetGpio(fanCtrl, false);
    }
    else if (heatSinkTemperature > 55)
    {
    	stmSetGpio(fanCtrl, true);
    }
    else if (heatSinkTemperature > MAX_TEMPERATURE) // Safety mode to avoid overheating of the board
    {
    	unsigned long now = HAL_GetTick();
    	if (now - previous > 2000)	// 2000ms. Should be aligned with the control period of heater.
    	{
    		previous = now;
    		adjustPWMDown();
    	}
    }
}

void ACBoardInit(ADC_HandleTypeDef* hadc)
{
    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));
    initCAProtocol(&caProto, usb_cdc_rx);
    GpioInit();
}

void ACBoardLoop(const char *bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(printCurrentArray);
    heatSinkLoop();

    // Toggle pins if needed when in pwm mode
    heaterLoop();
}
