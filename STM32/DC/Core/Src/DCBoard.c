/*
 * DCBoard.c
 */

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "circular_buffer.h"
#include "USBprint.h"
#include "usb_cdc_fops.h"
#include "systemInfo.h"
#include "DCBoard.h"
#include "ADCMonitor.h"
#include "si7051.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "time32.h"

#define ADC_CHANNELS    6
#define ACTUATIONPORTS 4
#define ADC_CHANNEL_BUF_SIZE    400

// PWM control
#define TURNONPWM 999
#define TURNOFFPWM 0
#define MAX_PWM TURNONPWM

// Private static function declarations.
static void CAallOn(bool isOn, int duration_ms);
static void CAportState(int port, bool state, int percent, int duration);

// Local variables.
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL,
        .allOn = CAallOn,
        .portState = CAportState
};

/* General */
static int actuationDuration[ACTUATIONPORTS] = { 0 };
static uint32_t actuationStart[ACTUATIONPORTS] = { 0 };
static bool port_state[ACTUATIONPORTS] = { 0 };
static uint32_t ccr_states[ACTUATIONPORTS] = { 0 };

// Temperature handling
static I2C_HandleTypeDef *hi2c = NULL;

static double meanCurrent(const int16_t *pData, uint16_t channel)
{
    // ADC to current calibration values
    const float current_scalar = -0.011;
    const float current_bias = 23.43; // Offset calibrated to USB hubs.

    return current_scalar * ADCMean(pData, channel) + current_bias;
}

WWDG_HandleTypeDef* hwwdg_ = NULL;
static void printResult(int16_t *pBuffer, int noOfChannels, int noOfSamples)
{
	// Watch dog refresh. Triggers reset if reset after <90ms or >110ms.
	// Ensures ADC sampling is performed with correct timing.
	HAL_WWDG_Refresh(hwwdg_);
	if (!isComPortOpen())
		return;

    float temp;
    if (si7051Temp(hi2c, &temp) != HAL_OK) temp = 10000;

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %d",
            meanCurrent(pBuffer, 0), meanCurrent(pBuffer, 1),
            meanCurrent(pBuffer, 2), meanCurrent(pBuffer, 3), temp,
            ADCrms(pBuffer, 5), ADCmax(pBuffer, 5));
}

static void setPWMPin(int pinNumber, int pwmState, int duration)
{

    if (pinNumber == 0)
    {
        TIM5->CCR1 = pwmState;
    }
    else if (pinNumber == 1)
    {
        TIM5->CCR2 = pwmState;
    }
    else if (pinNumber == 2)
    {
        TIM5->CCR3 = pwmState;
    }
    else if (pinNumber == 3)
    {
        TIM5->CCR4 = pwmState;
    }

    actuationStart[pinNumber] = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
    port_state[pinNumber] = 1;
}

static void pinWrite(int pinNumber, bool turnOn)
{
    // Normal turn off is done by choosing min PWM value i.e. pin always low.
    if (pinNumber == 0)
    {
        TIM5->CCR1 = (turnOn) ? TURNONPWM : TURNOFFPWM;
    }
    else if (pinNumber == 1)
    {
        TIM5->CCR2 = (turnOn) ? TURNONPWM : TURNOFFPWM;
    }
    else if (pinNumber == 2)
    {
        TIM5->CCR3 = (turnOn) ? TURNONPWM : TURNOFFPWM;
    }
    else if (pinNumber == 3)
    {
        TIM5->CCR4 = (turnOn) ? TURNONPWM : TURNOFFPWM;
    }
}

// Turn on all pins.
static void allOn()
{
    for (int pinNumber = 0; pinNumber < 4; pinNumber++)
    {
        pinWrite(pinNumber, SET);
        actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
        port_state[pinNumber] = 1;
        ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
    }
}

static void turnOnPin(int pinNumber)
{
    pinWrite(pinNumber, SET);
    actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
    port_state[pinNumber] = 1;
    ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}

static void turnOnPinDuration(int pinNumber, int duration)
{
    pinWrite(pinNumber, SET);
    actuationStart[pinNumber] = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    port_state[pinNumber] = 1;
    ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}

// Shuts off all pins.
static void allOff()
{
    for (int pinNumber = 0; pinNumber < 4; pinNumber++)
    {
        pinWrite(pinNumber, RESET);
        actuationDuration[pinNumber] = 0;
        port_state[pinNumber] = 0;
        ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
    }
}

static void turnOffPin(int pinNumber)
{
    pinWrite(pinNumber, RESET);
    actuationDuration[pinNumber] = 0;
    port_state[pinNumber] = 0;
    ccr_states[pinNumber] = *(&TIM5->CCR1 + pinNumber);
}

typedef struct ActuationInfo
{
    int pin; // pins 0-3 are interpreted as single ports - pin '-1' is interpreted as all
    int percent;
    int timeOn; // time on is in seconds - timeOn '-1' is interpreted as indefinitely
    bool isInputValid;
} ActuationInfo;
static void actuatePins(ActuationInfo actuationInfo)
{
    if (!actuationInfo.isInputValid)
    {
        return;
    }

    if (actuationInfo.timeOn == -1 && (actuationInfo.percent == 100 || actuationInfo.percent == 0))
    {
        // pX on or pX off (timeOn == -1 means indefinite)
        if (actuationInfo.percent == 0)
        {
            turnOffPin(actuationInfo.pin);
        }
        else
        {
            turnOnPin(actuationInfo.pin);
        }
    }
    else if (actuationInfo.timeOn != -1 && actuationInfo.percent == 100)
    {
        // pX on YY
        turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
    }
    else if (actuationInfo.timeOn == -1 && actuationInfo.percent != 0 && actuationInfo.percent != 100)
    {
        // pX on ZZZ%
        int pwmState = (actuationInfo.percent * MAX_PWM) / 100;
        setPWMPin(actuationInfo.pin, pwmState, 0);
    }
    else if (actuationInfo.timeOn != -1 && actuationInfo.percent != 100)
    {
        // pX on YY ZZZ%
        int pwmState = (actuationInfo.percent * MAX_PWM) / 100;
        setPWMPin(actuationInfo.pin, pwmState, actuationInfo.timeOn);
    }
}

static void CAallOn(bool isOn, int duration_ms)
{
    /* Unused argument required for compatibility with AC board */
    (void) duration_ms;

    (isOn) ? allOn() : allOff();
}
static void CAportState(int port, bool state, int percent, int duration)
{
    actuatePins((ActuationInfo) { port - 1, percent, 1000*duration, true });
}

static void autoOff()
{
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < 4; i++)
    {
        if (tdiff_u32(now, actuationStart[i]) > actuationDuration[i] && actuationDuration[i] != 0)
        {
            turnOffPin(i);
        }
    }
}

static void handleButtonPress()
{
    // Button ports
    GPIO_TypeDef *button_ports[] = { Btn_2_GPIO_Port, Btn_1_GPIO_Port,
            Btn_4_GPIO_Port, Btn_3_GPIO_Port };
    const uint16_t buttonPins[] = { Btn_2_Pin, Btn_1_Pin, Btn_4_Pin, Btn_3_Pin };

    for (int i = 0; i < 4; i++)
    {
        if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 0)
        {
            pinWrite(i, SET);
        }
        else if (HAL_GPIO_ReadPin(button_ports[i], buttonPins[i]) == 1)
        {
            if (port_state[i] == 1)
            {
                unsigned long now = HAL_GetTick();
                int duration = actuationDuration[i] - (int) tdiff_u32(now, actuationStart[i]);
                setPWMPin(i, ccr_states[i], duration);
            }
            else
            {
                pinWrite(i, RESET);
            }
        }
    }
}

static void checkButtonPress()
{
    static uint32_t lastCheckButtonTime = 0;
    const uint32_t tsButton = 100;

    if (tdiff_u32(HAL_GetTick(), lastCheckButtonTime) > tsButton)
    {
        lastCheckButtonTime = HAL_GetTick();
        handleButtonPress();
    }
}

// Public member functions.
void DCBoardInit(ADC_HandleTypeDef *_hadc, I2C_HandleTypeDef *_hi2c, WWDG_HandleTypeDef* hwwdg)
{
    BoardType board;
    if (getBoardInfo(&board, NULL) || board != DC_Board)
        return;


    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];
    ADCMonitorInit(_hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(ADCBuffer[0]));
    initCAProtocol(&caProto, usb_cdc_rx);
    hi2c = _hi2c;
    hwwdg_ = hwwdg;
}

void DCBoardLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(printResult);

    // Turn off pins if they have run for requested time
    autoOff();
    checkButtonPress();
}

