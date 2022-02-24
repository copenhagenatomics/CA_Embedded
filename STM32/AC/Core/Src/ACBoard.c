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
#include "inputValidation.h"
#include "HeatCtrl.h"
#include "ADCMonitor.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "HAL_otp.h"

#define ADC_CHANNELS	5
#define ADC_CHANNEL_BUF_SIZE	400

/* GPIO settings. */
static struct
{
    StmGpio heater;
    StmGpio button;
} heaterPorts[4];

// Forward declare functions.
static void handlePinInput(const char *input);

static CAProtocolCtx caProto =
{
        .undefined = handlePinInput,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

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

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f", ADCtoCurrent(ADCrms(pData, 1)),
            ADCtoCurrent(ADCrms(pData, 2)), ADCtoCurrent(ADCrms(pData, 3)),
            ADCtoCurrent(ADCrms(pData, 4)),
            ADCtoTemperature(ADCMean(pData, 0)));
}

void actuatePins(struct actuationInfo actuationInfo, const char *inputBuffer)
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
    else
    {
        HALundefined(inputBuffer); // should never reach this, but is implemented for potentially unknown errors.
    }
}

static void handlePinInput(const char *input)
{
    struct actuationInfo actuationInfo = parseAndValidateInput(input);

    if (actuationInfo.isInputValid) actuatePins(actuationInfo, input);
    else
        HALundefined(input);
}

void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];

    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}

void clearLineAndBuffer()
{
    // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
    USBnprintf("reconnected");
    usb_cdc_rx_flush();
}

void ACBoardInit(ADC_HandleTypeDef* hadc)
{
    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2]; // array for all ADC readings, filled by DMA.

    ADCMonitorInit(hadc, ADCBuffer, sizeof(ADCBuffer)/sizeof(int16_t));
    GpioInit();
}

void ACBoardLoop()
{
    static bool isFirstWrite = true;

    handleUserInputs();
    ADCMonitorLoop(printCurrentArray);

    if (isComPortOpen())
    {
        // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
        if (isFirstWrite)
        {
            clearLineAndBuffer();
            isFirstWrite = false;
        }
    }
    else
    {
        isFirstWrite = true;
    }

    // Toggle pins if needed when in pwm mode
    heaterLoop();
}
