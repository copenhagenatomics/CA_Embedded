/*
 * DCBoard.c
 */

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "main.h"
#include "circular_buffer.h"
#include "USBprint.h"
#include "systemInfo.h"
#include "DCBoard.h"
#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "time32.h"
#include "StmGpio.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS    6
#define ACTUATIONPORTS 6
#define ADC_CHANNEL_BUF_SIZE    400

// PWM control
#define TURNONPWM 999
#define TURNOFFPWM 0
#define MAX_PWM TURNONPWM

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void CAallOn(bool isOn, int duration_ms);
static void CAportState(int port, bool state, int percent, int duration);
static volatile uint32_t* getTimerCCR(int pinNumber);
static void printDcStatus();
static void updateBoardStatus();
static void gpioInit ();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = CAPrintHeader,
        .printStatus = printDcStatus,
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

/* Button ports */
static GPIO_TypeDef *button_ports[] = { Btn_1_GPIO_Port, Btn_2_GPIO_Port, Btn_3_GPIO_Port, 
                                        Btn_4_GPIO_Port, Btn_5_GPIO_Port, Btn_6_GPIO_Port};
static const uint16_t buttonPins[] = { Btn_1_Pin, Btn_2_Pin, Btn_3_Pin, 
                                       Btn_4_Pin, Btn_5_Pin, Btn_6_Pin };
static StmGpio buttonGpio[ACTUATIONPORTS] = {0};
static StmGpio sense24v;

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
** @brief Verbose print of the DC board status
*/
static void printDcStatus()
{
    static char buf[600] = { 0 };
    int len = 0;

    for (int i = 0; i < ACTUATIONPORTS; i++) {
        len += snprintf(&buf[len], sizeof(buf) - len, "Port %d: On: %d, PWM percent: %" PRIu32 "\r\n", 
                        i, (int)port_state[i], *getTimerCCR(i));
    }

    writeUSB(buf, len);
}

/*!
** @brief Updates the global error/status object.
**
** Adds the port status bits into the error field, and clears the error bit if there are no
** more errors.
*/
static void updateBoardStatus() 
{
    /* If a port is turned on or not */
    for(int i = 0; i < ACTUATIONPORTS; i++)
    {
        *getTimerCCR(i) != TURNOFFPWM ? bsSetField(DC_BOARD_PORT_x_STATUS_Msk(i)) : bsClearField(DC_BOARD_PORT_x_STATUS_Msk(i));
    }

    /* If 24V is not present */
    if(!stmGetGpio(sense24v)) {
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else {
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }

    /* Clear the error mask if there are no error bits set any more. This logic could be done when
    ** the (other) error bits are cleared, but doing here means it only needs to be done once */
    bsClearError(DC_BOARD_No_Error_Msk);
}

static double meanCurrent(const int16_t *pData, uint16_t channel)
{
    // ADC to current calibration values
    const float current_scalar = ((3.3 / 4096.0) / 0.264); // From ACS725LLCTR-05AB datasheet
    const float current_bias   = - 6.25;                        // Offset calibrated to USB hubs.

    return current_scalar * ADCMean(pData, channel) + current_bias;
}

WWDG_HandleTypeDef* hwwdg_ = NULL;
static void printResult(int16_t *pBuffer, int noOfChannels, int noOfSamples)
{
    // Watch dog refresh. Triggers reset if reset after <90ms or >110ms.
    // Ensures ADC sampling is performed with correct timing.
    HAL_WWDG_Refresh(hwwdg_);
    if (!isUsbPortOpen())
    {
        return;
    }

    /* If the version is incorrect, there is no point printing data or doing maths */
    if (bsGetStatus() & BS_VERSION_ERROR_Msk)
    {
        USBnprintf("0x%x", bsGetStatus());
        return;
    }

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08x",
            meanCurrent(pBuffer, 0), meanCurrent(pBuffer, 1),
            meanCurrent(pBuffer, 2), meanCurrent(pBuffer, 3),
            meanCurrent(pBuffer, 4), meanCurrent(pBuffer, 5),
            bsGetStatus());
}

static void setPWMPin(int pinNumber, int pwmState, int duration)
{
    volatile uint32_t* ccr = getTimerCCR(pinNumber);
    *ccr = pwmState;
    actuationStart[pinNumber] = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    ccr_states[pinNumber] = *ccr;
    port_state[pinNumber] = 1;
}

static void pinWrite(int pinNumber, bool turnOn)
{
    // Normal turn off is done by choosing min PWM value i.e. pin always low.
    *(getTimerCCR(pinNumber)) = (turnOn) ? TURNONPWM : TURNOFFPWM;
}

// Turn on all pins.
static void allOn()
{
    for (int pinNumber = 0; pinNumber < ACTUATIONPORTS; pinNumber++)
    {
        pinWrite(pinNumber, true);
        actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
        port_state[pinNumber] = 1;
        ccr_states[pinNumber] = *(getTimerCCR(pinNumber));
    }
}

static void turnOnPin(int pinNumber)
{
    pinWrite(pinNumber, true);
    actuationDuration[pinNumber] = 0; // actuationDuration=0 since it should be on indefinitely
    port_state[pinNumber] = 1;
    ccr_states[pinNumber] = *(getTimerCCR(pinNumber));
}

static void turnOnPinDuration(int pinNumber, int duration)
{
    pinWrite(pinNumber, true);
    actuationStart[pinNumber] = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    port_state[pinNumber] = 1;
    ccr_states[pinNumber] = *(getTimerCCR(pinNumber));
}

// Shuts off all pins.
static void allOff()
{
    for (int pinNumber = 0; pinNumber < ACTUATIONPORTS; pinNumber++)
    {
        pinWrite(pinNumber, false);
        actuationDuration[pinNumber] = 0;
        port_state[pinNumber] = 0;
        ccr_states[pinNumber] = *(getTimerCCR(pinNumber));
    }
}

static void turnOffPin(int pinNumber)
{
    pinWrite(pinNumber, false);
    actuationDuration[pinNumber] = 0;
    port_state[pinNumber] = 0;
    ccr_states[pinNumber] = *(getTimerCCR(pinNumber));
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
    else if (actuationInfo.pin < 0 || actuationInfo.pin >= ACTUATIONPORTS) {
        return;
    }

    // pX on or pX off (timeOn == -1 means indefinite)
    if (actuationInfo.timeOn == -1000 && actuationInfo.percent == 100) {
        turnOnPin(actuationInfo.pin);
    }
    else if (actuationInfo.timeOn == -1000 && actuationInfo.percent == 0) {
        turnOffPin(actuationInfo.pin);
    }
    else if (actuationInfo.timeOn != -1000 && actuationInfo.percent == 100)
    {
        // pX on YY
        turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
    }
    else if (actuationInfo.timeOn == -1000 && actuationInfo.percent != 0 && actuationInfo.percent != 100)
    {
        // pX on ZZZ%
        int pwmState = (actuationInfo.percent * MAX_PWM) / 100;
        setPWMPin(actuationInfo.pin, pwmState, 0);
    }
    else if (actuationInfo.timeOn != -1000 && actuationInfo.percent != 100)
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

    for (int i = 0; i < ACTUATIONPORTS; i++)
    {
        if (tdiff_u32(now, actuationStart[i]) > actuationDuration[i] && actuationDuration[i] != 0)
        {
            turnOffPin(i);
        }
    }
}

static void handleButtonPress()
{
    for (int i = 0; i < ACTUATIONPORTS; i++)
    {
        if (stmGetGpio(buttonGpio[i]) == 0)
        {
            pinWrite(i, true);
        }
        else if (stmGetGpio(buttonGpio[i]) == 1)
        {
            if (port_state[i] == 1)
            {
                unsigned long now = HAL_GetTick();
                int duration = actuationDuration[i] - (int) tdiff_u32(now, actuationStart[i]);
                setPWMPin(i, ccr_states[i], duration);
            }
            else
            {
                pinWrite(i, false);
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

/*!
** @brief Returns a pointer to the correct CCR register
**
** @param[in] pinNumber Number of DC port to get corresponding timer for
**
** @return Address of CCR register, or 0 if an incorrect pinNumber is given
*/
static volatile uint32_t* getTimerCCR(int pinNumber)
{
    /* The map of DC ports to Timer CCR registers has been tested and confirmed on a V3.0 board */
    switch (pinNumber)
    {
        case 0: return &(TIM4->CCR2);
        case 1: return &(TIM4->CCR1);
        case 2: return &(TIM5->CCR1);
        case 3: return &(TIM5->CCR2);
        case 4: return &(TIM5->CCR4);
        case 5: return &(TIM5->CCR3);
        default: return (uint32_t*)0x00000000;
    }
}

/*!
** @brief Initialises GPIO in the system
*/
static void gpioInit () {
    for(int i = 0; i < 6; i++) {
        stmGpioInit(&buttonGpio[i], button_ports[i], buttonPins[i], STM_GPIO_INPUT);
    }
    stmGpioInit(&sense24v, SENSE_24V_GPIO_Port, SENSE_24V_Pin, STM_GPIO_INPUT);
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

void DCBoardInit(ADC_HandleTypeDef *_hadc, WWDG_HandleTypeDef* hwwdg)
{
    setFirmwareBoardType(DC_Board);
    setFirmwareBoardVersion((pcbVersion){3, 1});

    /* Always allow for DFU also if programmed on non-matching board or PCB version. */
    initCAProtocol(&caProto, usbRx);
    
    BoardType board;
    if (getBoardInfo(&board, NULL) || board != DC_Board) 
    {
        bsSetError(BS_VERSION_ERROR_Msk);
    }

    // Pin out has changed from PCB V3.0 - older versions need other software.
    pcbVersion ver;
    if (getPcbVersion(&ver) || ver.major < 3 || ver.minor < 1)
    {
        bsSetError(BS_VERSION_ERROR_Msk);
    }

    gpioInit();

    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];
    ADCMonitorInit(_hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(ADCBuffer[0]));
    hwwdg_ = hwwdg;
}

/*!
** @brief Loop function called repeatedly throughout
** 
** * Responds to user input
** * Checks for ADC buffer switches (the ADC sample rate is synchronised with USB print rate - the 
**   USB print rate should be 10 Hz, so every 400 ADC samples the buffer switches).
*/
void DCBoardLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();

    ADCMonitorLoop(printResult);

    // Turn off pins if they have run for requested time
    autoOff();
    checkButtonPress();
}
