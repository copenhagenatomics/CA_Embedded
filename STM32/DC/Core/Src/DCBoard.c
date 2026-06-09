/**
 ******************************************************************************
 * @file    DCBoard.c
 * @brief   Main file for DC board
 * @date:   6 Oct 2021
 * @author: Matias
 ******************************************************************************
 */

#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolACDC.h"
#include "CAProtocolStm.h"
#include "DCBoard.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "flashHandler.h"
#include "githash.h"
#include "main.h"
#include "pcbversion.h"
#include "systemInfo.h"
#include "time32.h"
#include "uptime.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         7  // Order: Hall1 - Hall6, 24V sense
#define ADC_CHANNEL_BUF_SIZE 400
#define INPUT_V_CHANNEL_IDX  6

// PWM control
#define TURNONPWM  999
#define TURNOFFPWM 0
#define MAX_PWM    TURNONPWM

/* Port control flags */
#define PORT_STATE_ON_SW  0x01U
#define PORT_STATE_ON_BTN 0x02U

#define USB_COMMS_TIMEOUT_MS 5000

#define UNDER_VOLTAGE_THRESHOLD 10
#define OVER_VOLTAGE_THRESHOLD  27

#define MAX_MEASURABLE_CURRENT        6.25f
#define EFUSE_DEFAULT_CURRENT_LIMIT_A (MAX_MEASURABLE_CURRENT - 0.25)

/* Custom uptime channel indices (after the NUM_DEFAULT_CHANNELS defaults) */
#define DC_NUM_CUSTOM_UPTIME_CHANNELS (DC_BOARD_NUM_PORTS * 2)
#define DC_UPTIME_FULL_ON_CH(port)    (NUM_DEFAULT_CHANNELS + (port) * 2)
#define DC_UPTIME_PWM_ON_CH(port)     (NUM_DEFAULT_CHANNELS + (port) * 2 + 1)
#define UPTIME_1_MIN_MS              60000

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void DCInputHandler(const char* input);
static void DCUptimeHandler(const char* input);
static void CAallOn(bool isOn, int duration_ms);
static void CAportState(int port, bool state, int percent, int duration);
static void DCcalibration(int noOfCalibrations, const CACalibration* calibrations);
static void DCcalibrationRW(bool write);
static void efuseLoop(const double* currents);
static void clearOvercurrentFields(int port);
static volatile uint32_t* getTimerCCR(int pinNumber);
static void printDcStatus();
static void updateBoardStatus();
static double meanCurrent(const int16_t* pData, uint16_t channel);
static double adcToInputVoltage(double adcMean);
static void printResult(int16_t* pBuffer, int noOfChannels, int noOfSamples);
static void setPWMPin(int pinNumber, int pwmState, int duration);
static void allOn(int duration);
static void allOff();
static void turnOnPin(int pinNumber);
static void turnOnPinDuration(int pinNumber, int duration);
static void turnOffPin(int pinNumber);
static void gpioInit();
static void handlePorts();
static void updatePortUptime();
static void autoOff();
static void handleButtonPress();
static void checkButtonPress();
static void printDCHeader();
static bool efuseCurrentInRange(double current);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static ACDCProtocolCtx dcProto = {.allOn = CAallOn, .portState = CAportState};

static CAProtocolCtx caProto = {.undefined        = DCInputHandler,
                                .printHeader      = printDCHeader,
                                .printStatus      = printDcStatus,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = DCcalibration,
                                .calibrationRW    = DCcalibrationRW,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL,
                                .uptime           = DCUptimeHandler};

/* General */
static int actuationDuration[DC_BOARD_NUM_PORTS]   = {0};
static uint32_t actuationStart[DC_BOARD_NUM_PORTS] = {0};
static uint32_t port_state[DC_BOARD_NUM_PORTS]     = {0};
static uint32_t ccr_states[DC_BOARD_NUM_PORTS]     = {0};

/* E-fuse */
static float* efuseCurrentLimits = NULL;

/* HAL handles */
static CRC_HandleTypeDef*  hcrc_  = NULL;

/* Button ports */
static GPIO_TypeDef* button_ports[]       = {Btn_1_GPIO_Port, Btn_2_GPIO_Port, Btn_3_GPIO_Port,
                                             Btn_4_GPIO_Port, Btn_5_GPIO_Port, Btn_6_GPIO_Port};
static const uint16_t buttonPins[]        = {Btn_1_Pin, Btn_2_Pin, Btn_3_Pin,
                                             Btn_4_Pin, Btn_5_Pin, Btn_6_Pin};
static StmGpio buttonGpio[DC_BOARD_NUM_PORTS] = {};

static float inputVoltage = 24;

/* Per-port uptime tracking */
static const char* dcUptimeChannelDesc[DC_NUM_CUSTOM_UPTIME_CHANNELS] = {
    "Port 1 full on minutes", "Port 1 PWM on minutes",
    "Port 2 full on minutes", "Port 2 PWM on minutes",
    "Port 3 full on minutes", "Port 3 PWM on minutes",
    "Port 4 full on minutes", "Port 4 PWM on minutes",
    "Port 5 full on minutes", "Port 5 PWM on minutes",
    "Port 6 full on minutes", "Port 6 PWM on minutes",
};
static uint32_t prevCCR[DC_BOARD_NUM_PORTS]            = {0};
static uint32_t portFullOnCounter[DC_BOARD_NUM_PORTS] = {0};
static uint32_t portPwmOnCounter[DC_BOARD_NUM_PORTS]  = {0};
static uint32_t last_check = 0;

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
** @brief Prints the DC board header information
*/
static void printDCHeader() {
    CAPrintHeader();
    DCcalibrationRW(false);
}

/*!
** @brief Call command handler for DC board
*/
static void DCInputHandler(const char* input) {
    ACDCInputHandler(&dcProto, input);
}

/*!
** @brief Handles the "uptime" command
*/
static void DCUptimeHandler(const char* input) {
    uptime_inputHandler(input, printDCHeader);
}

/*!
**  @brief Verifies a current is within the efuse range
*/
static bool efuseCurrentInRange(double current) {
    return (current > 0) && (current < MAX_MEASURABLE_CURRENT);
}

/*!
** @brief Updates calibration memory with per-channel e-fuse current limits.
**        Command format: "CAL <N>,<limit_amps>,0,0"
*/
static void DCcalibration(int noOfCalibrations, const CACalibration* calibrations) {
    for (int i = 0; i < noOfCalibrations; i++) {
        int port = calibrations[i].port;
        if (port >= 1 && port <= DC_BOARD_NUM_PORTS && efuseCurrentInRange(calibrations[i].alpha)) {
            efuseCurrentLimits[port - 1] = (float)calibrations[i].alpha;
        }
        else {
            USBnprintf("Invalid calibration input: %d,%.2f,0,0\r\n", port, calibrations[i].alpha);
        }
    }
}

/*!
** @brief Reads or writes e-fuse current limits to/from flash.
*/
static void DCcalibrationRW(bool write) {
    if (write) {
        fhSaveDeposit(hcrc_);
    }
    else {
        char buf[300];
        int len = 0;
        CA_SNPRINTF(buf, len, "Calibration: CAL");
        for (int ch = 0; ch < DC_BOARD_NUM_PORTS; ch++) {
            CA_SNPRINTF(buf, len, " %d,%.2f,0,0", ch + 1, efuseCurrentLimits[ch]);
        }
        CA_SNPRINTF(buf, len, "\r\n");
        writeUSB(buf, len);
    }
}

/*!
** @brief Clears the overcurrent fields for a specific port and clears the generic overcurrent
**        error if no ports are currently in overcurrent anymore.
*/
static void clearOvercurrentFields(int port) {
    bsClearField(DC_EFUSE_OVERCURRENT_Msk(port));
    if (!(bsGetField(DC_EFUSE_OVERCURRENT_ALL_Msk))) {
        bsClearField(BS_OVER_CURRENT_Msk);
    }
}

/*!
** @brief Checks per-channel current and trips the e-fuse if exceeded.
*/
static void efuseLoop(const double* currents) {
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        if (currents[i] > efuseCurrentLimits[i]) {
            turnOffPin(i);
            port_state[i] &= ~PORT_STATE_ON_BTN;
            bsSetError(DC_EFUSE_OVERCURRENT_Msk(i + 1));
            bsSetError(BS_OVER_CURRENT_Msk);
        }
    }
}

/*!
** @brief Verbose print of the DC board status
*/
static void printDcStatus() {
    static char buf[600] = {0};
    int len              = 0;

    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        CA_SNPRINTF(buf, len, "Port %d: On: %" PRIu32 ", PWM percent: %" PRIu32 "\r\n", i,
                    port_state[i], *getTimerCCR(i));
    }

    writeUSB(buf, len);
}

/*!
** @brief Updates the global error/status object.
**
** Adds the port status bits into the error field, and clears the error bit if there are no
** more errors.
*/
static void updateBoardStatus() {
    /* If a port is turned on or not */
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        *getTimerCCR(i) != TURNOFFPWM ? bsSetField(DC_BOARD_PORT_x_STATUS_Msk(i))
                                      : bsClearField(DC_BOARD_PORT_x_STATUS_Msk(i));
    }

    if (inputVoltage < UNDER_VOLTAGE_THRESHOLD) {
        // Below ~10V
        bsSetError(BS_UNDER_VOLTAGE_Msk);
    }
    else if (inputVoltage > OVER_VOLTAGE_THRESHOLD) {
        // Above ~27V
        bsSetError(BS_OVER_VOLTAGE_Msk);
    }
    else {
        bsClearField(BS_OVER_VOLTAGE_Msk);
        bsClearField(BS_UNDER_VOLTAGE_Msk);
    }

    /* Clear the error mask if there are no error bits set any more. This logic could be done when
    ** the (other) error bits are cleared, but doing here means it only needs to be done once */
    bsClearError(DC_BOARD_No_Error_Msk);
}

static double meanCurrent(const int16_t* pData, uint16_t channel) {
    // ADC to current calibration values
    const float CURRENT_SCALAR = ((3.3 / 4096.0) / 0.264);  // From ACS725LLCTR-05AB datasheet
    const float CURRENT_BIAS   = -6.25;                     // Offset calibrated to USB hubs.

    return CURRENT_SCALAR * ADCMean(pData, channel) + CURRENT_BIAS;
}

static double adcToInputVoltage(double adcMean) {
    /* Check input voltage - Values are derived from experimental data:
    ** https://docs.google.com/spreadsheets/d/1Ol6N_XpXo1H1fYc5LJ2H3Ol09gSS1-E9HtE2mK8wYmI/edit?gid=0#gid=0
    **
    ** NOTE: Calibration values can vary alot from board to board meaning voltage calculation
    **       can be as high as ±2V in the high range. Hence, the input voltage is not very
    **       accurate and the output should inspected more carefully if used for diagnostics. */
    const float VOLTAGE_QUAD   = -1.31e-5;
    const float VOLTAGE_SCALAR = 0.0373;
    const float VOLTAGE_BIAS   = 3.17;

    return (VOLTAGE_QUAD * adcMean + VOLTAGE_SCALAR) * adcMean + VOLTAGE_BIAS;
}

static void printResult(int16_t* pBuffer, int noOfChannels, int noOfSamples) {
    static uint32_t port_close_time = 0;

    /* If the USB port is not open, no messages should be printed. Also if the USB port has been
    ** closed for more than a timeout, everything should be turned off as a safety measure */
    if (!isUsbPortOpen()) {
        if (port_close_time == 0) {
            port_close_time = HAL_GetTick();
        }
        else if ((HAL_GetTick() - port_close_time) >= USB_COMMS_TIMEOUT_MS) {
            port_close_time = 0;
            allOff();
        }
        return;
    }

    /* If the version is incorrect, there is no point printing data or doing maths */
    if (bsGetStatus() & BS_VERSION_ERROR_Msk) {
        USBnprintf("0x%08x\r\n", bsGetStatus());
        return;
    }

    inputVoltage = adcToInputVoltage(ADCMean(pBuffer, INPUT_V_CHANNEL_IDX));
    setBoardVoltage(inputVoltage);

    double currents[DC_BOARD_NUM_PORTS];
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        currents[i] = meanCurrent(pBuffer, i);
    }

    USBnprintf("%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, 0x%08x\r\n", currents[0], currents[1],
               currents[2], currents[3], currents[4], currents[5], bsGetStatus());

    efuseLoop(currents);
}

static void setPWMPin(int pinNumber, int pwmState, int duration) {
    actuationStart[pinNumber]    = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    ccr_states[pinNumber]        = pwmState;
    port_state[pinNumber] |= PORT_STATE_ON_SW;
}

// Turn on all pins.
static void allOn(int duration) {
    if (duration <= 0) {
        char buf[20] = {0};
        snprintf(buf, 20, "all on %d", duration);
        HALundefined(buf);
    }
    else {
        for (int pinNumber = 0; pinNumber < DC_BOARD_NUM_PORTS; pinNumber++) {
            actuationStart[pinNumber]    = HAL_GetTick();
            actuationDuration[pinNumber] = duration;
            port_state[pinNumber] |= PORT_STATE_ON_SW;
            ccr_states[pinNumber] = TURNONPWM;
        }
    }
}

static void turnOnPin(int pinNumber) {
    actuationDuration[pinNumber] = 0;  // actuationDuration=0 since it should be on indefinitely
    port_state[pinNumber] |= PORT_STATE_ON_SW;
    ccr_states[pinNumber] = TURNONPWM;
}

static void turnOnPinDuration(int pinNumber, int duration) {
    actuationStart[pinNumber]    = HAL_GetTick();
    actuationDuration[pinNumber] = duration;
    port_state[pinNumber] |= PORT_STATE_ON_SW;
    ccr_states[pinNumber] = TURNONPWM;
}

// Shuts off all pins.
static void allOff() {
    for (int pinNumber = 0; pinNumber < DC_BOARD_NUM_PORTS; pinNumber++) {
        actuationDuration[pinNumber] = 0;
        port_state[pinNumber] &= ~PORT_STATE_ON_SW;
        ccr_states[pinNumber] = TURNOFFPWM;
    }
}

static void turnOffPin(int pinNumber) {
    actuationDuration[pinNumber] = 0;
    port_state[pinNumber] &= ~PORT_STATE_ON_SW;
    ccr_states[pinNumber] = TURNOFFPWM;
}

typedef struct ActuationInfo {
    int pin;  // pins 0-5 are interpreted as single ports
    int percent;
    int timeOn;  // time on is in seconds - timeOn '-1' is interpreted as indefinitely
} ActuationInfo;
static void actuatePins(ActuationInfo actuationInfo) {
    if (actuationInfo.pin < 0 || actuationInfo.pin >= DC_BOARD_NUM_PORTS) {
        char buf[30] = {0};
        snprintf(buf, 30, "Invalid Pin: %d", actuationInfo.pin + 1);
        HALundefined(buf);
        return;
    }

    // pX on or pX off (timeOn == -1 means indefinite)
    if (actuationInfo.timeOn == -1000 && actuationInfo.percent == 100) {
        turnOnPin(actuationInfo.pin);
    }
    else if (actuationInfo.timeOn == -1000 && actuationInfo.percent == 0) {
        turnOffPin(actuationInfo.pin);
    }
    else if (actuationInfo.timeOn != -1000 && actuationInfo.percent == 100) {
        // pX on YY
        turnOnPinDuration(actuationInfo.pin, actuationInfo.timeOn);
    }
    /* Note: conditions in comments met by implication */
    else if (actuationInfo.timeOn ==
             -1000) /* && actuationInfo.percent != 0 && actuationInfo.percent != 100 */
    {
        // pX on ZZZ%
        int pwmState = (actuationInfo.percent * MAX_PWM) / 100;
        setPWMPin(actuationInfo.pin, pwmState, 0);
    }
    else if (actuationInfo.timeOn != -1000 &&
             actuationInfo.percent != 0) /* && actuationInfo.percent != 100 */
    {
        // pX on YY ZZZ%
        int pwmState = (actuationInfo.percent * MAX_PWM) / 100;
        setPWMPin(actuationInfo.pin, pwmState, actuationInfo.timeOn);
    }
}

static void CAallOn(bool isOn, int duration) {
    if (isOn) {
        for (int i = 1; i <= DC_BOARD_NUM_PORTS; i++) {
            clearOvercurrentFields(i);
        }
        allOn(1000 * duration);
    }
    else {
        allOff();
    }
}

static void CAportState(int port, bool state, int percent, int duration) {
    clearOvercurrentFields(port);
    actuatePins((ActuationInfo){port - 1, percent, 1000 * duration});
}

static void autoOff() {
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        if (((int)tdiff_u32(now, actuationStart[i]) > actuationDuration[i]) &&
            actuationDuration[i] != 0) {
            turnOffPin(i);
        }
    }
}

static void handleButtonPress() {
    static int prev_gpio[DC_BOARD_NUM_PORTS] = {1, 1, 1, 1, 1, 1};

    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        int gpio = stmGetGpio(buttonGpio[i]);

        /* Button GPIO are pulled up, so "0" is a positive input (e.g. button is pressed) */
        if (gpio == 0 && prev_gpio[i] == 1) {
            /* Press edge: treat as a new command — clears any active e-fuse error */
            clearOvercurrentFields(i + 1);
            port_state[i] |= PORT_STATE_ON_BTN;
        }
        else if (gpio == 1 && prev_gpio[i] == 0) {
            /* Release edge */
            if (port_state[i] & PORT_STATE_ON_SW) {
                unsigned long now = HAL_GetTick();
                int duration      = actuationDuration[i] - (int)tdiff_u32(now, actuationStart[i]);
                setPWMPin(i, ccr_states[i], duration);
            }
            port_state[i] &= ~PORT_STATE_ON_BTN;
        }

        prev_gpio[i] = gpio;
    }
}

static void checkButtonPress() {
    static uint32_t lastCheckButtonTime = 0;
    const uint32_t tsButton             = 100;

    if (tdiff_u32(HAL_GetTick(), lastCheckButtonTime) > tsButton) {
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
static volatile uint32_t* getTimerCCR(int pinNumber) {
    /* The map of DC ports to Timer CCR registers has been tested and confirmed on a V3.2 board */
    switch (pinNumber) {
        case 0:
            return &(TIM2->CCR1);
        case 1:
            return &(TIM1->CCR2);
        case 2:
            return &(TIM1->CCR3);
        case 3:
            return &(TIM1->CCR1);
        case 4:
            return &(TIM2->CCR2);
        case 5:
            return &(TIM2->CCR3);
        default:
            return (uint32_t*)0x00000000;
    }
}

/*!
** @brief Increments per-port uptime counters based on the current CCR state.
**
** Detects transitions into each on-mode to avoid a spurious count on the first
** loop tick after the port turns on.
*/
static void updatePortUptime() {
    /* Calculate time difference since last check */
    uint32_t now = HAL_GetTick();
    uint32_t diff = now - last_check;
    last_check = now;

    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        uint32_t ccr     = *getTimerCCR(i);

        if (ccr == TURNONPWM) {
            /* Allows recording even quite small on-times */
            portFullOnCounter[i] += diff;
            if (portFullOnCounter[i] >= UPTIME_1_MIN_MS) {  // Convert milliseconds to minutes
                uptime_incChannel(DC_UPTIME_FULL_ON_CH(i));
                portFullOnCounter[i] -= UPTIME_1_MIN_MS;
            }
        }
        else if (ccr > 0) { /* PWM by implication */
            /* Allows recording even quite small on-times */
            portPwmOnCounter[i] += diff;
            if (portPwmOnCounter[i] >= UPTIME_1_MIN_MS) {  // Convert milliseconds to minutes
                uptime_incChannel(DC_UPTIME_PWM_ON_CH(i));
                portPwmOnCounter[i] -= UPTIME_1_MIN_MS;
            }
        }

        prevCCR[i] = ccr;
    }
}

/*!
** @brief Initialises GPIO in the system
*/
static void gpioInit() {
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        stmGpioInit(&buttonGpio[i], button_ports[i], buttonPins[i], STM_GPIO_INPUT_PULLUP);
    }
}

/*!
** @brief Uses port_state and ccr_state variables to determine if a pin should be on or not
*/
static void handlePorts() {
    if (!(bsGetStatus() & BS_VERSION_ERROR_Msk)) {
        for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
            if (port_state[i]) {
                if (port_state[i] & PORT_STATE_ON_BTN) {
                    *getTimerCCR(i) = TURNONPWM;
                }
                else {
                    *getTimerCCR(i) = ccr_states[i];
                }
            }
            else {
                *getTimerCCR(i) = 0;
            }
        }
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

void DCBoardInit(ADC_HandleTypeDef* _hadc, CRC_HandleTypeDef* hcrc, const char* bootMsg) {
    boardSetup(DC_Board, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, DC_BOARD_No_Error_Msk);
    /* Always allow for DFU also if programmed on non-matching board or PCB version. */
    initCAProtocol(&caProto, usbRx);

    gpioInit();

    static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];
    ADCMonitorInit(_hadc, ADCBuffer, sizeof(ADCBuffer) / sizeof(ADCBuffer[0]));
    hcrc_  = hcrc;

    uptime_init(hcrc, DC_NUM_CUSTOM_UPTIME_CHANNELS, dcUptimeChannelDesc, bootMsg, GIT_VERSION);
    last_check = HAL_GetTick();

    fhLoadDeposit(hcrc_);
    efuseCurrentLimits = fhGetCurrentLimits();
    for (int i = 0; i < DC_BOARD_NUM_PORTS; i++) {
        if (!isfinite(efuseCurrentLimits[i]) || !efuseCurrentInRange(efuseCurrentLimits[i])) {
            efuseCurrentLimits[i] = EFUSE_DEFAULT_CURRENT_LIMIT_A;
        }
    }
}

/*!
** @brief Loop function called repeatedly throughout
**
** * Responds to user input
** * Checks for ADC buffer switches (the ADC sample rate is synchronised with USB print rate - the
**   USB print rate should be 10 Hz, so every 400 ADC samples the buffer switches).
*/
void DCBoardLoop(const char* bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();

    ADCMonitorLoop(printResult);

    uptime_update();

    // Turn off pins if they have run for requested time
    autoOff();
    checkButtonPress();
    handlePorts();
    updatePortUptime();
}
