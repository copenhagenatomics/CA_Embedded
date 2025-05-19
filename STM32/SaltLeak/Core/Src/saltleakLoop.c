/*!
 * @file    saltleakLoop.c
 * @brief   This file contains the main program of Saltleak
 * @date    11/11/2021
 * @author  agp
 */

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "main.h"
#include "pcbversion.h"
#include "saltleakLoop.h"
#include "stm32f4xx_hal.h"
#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_CHANNELS         8     // Number of ADC channels used on the STM32
#define ADC_CHANNEL_BUF_SIZE 400   // 4 kHz sampling  rate -> 10 Hz
#define ADC_MAX              4095  // 12-bits
#define ANALOG_REF_VOLTAGE   3.3   // V

#define V_BOOST_NOMINAL 48.0  // V
#define VCC_NOMINAL     5.0   // V

#define NO_OF_SENSORS  6     // Number of salt leak sensors
#define LEAK_THRESHOLD 10.0  // kOhm  - Leak threshold

/* Specific Board Status register definitions */

// Boost voltage out of nominal value
#define BS_BOOST_ERROR_Pos 15U
#define BS_BOOST_ERROR_Msk (1UL << BS_BOOST_ERROR_Pos)

// Boost controller active
#define BS_BOOST_ACTIVE_Pos 14U
#define BS_BOOST_ACTIVE_Msk (1UL << BS_BOOST_ACTIVE_Pos)

// Boost converter enabled
#define BS_BOOST_PIN_HIGH_Pos 13U
#define BS_BOOST_PIN_HIGH_Msk (1UL << BS_BOOST_PIN_HIGH_Pos)

// Sensor state (sensorState_t)
#define BS_SENSOR_STATE_RANGE_Pos(x) (2 * (NO_OF_SENSORS - 1 - x))
#define BS_SENSOR_STATE_RANGE_Msk(x) (3UL << BS_SENSOR_STATE_RANGE_Pos(x))

// Used for defining which bits are errors, and which are statuses
#define SALTLEAK_ERRORS_Msk (BS_SYSTEM_ERRORS_Msk | BS_BOOST_ERROR_Msk)

typedef enum { NC_OR_BROKEN_STATE, BROKEN_STATE, LEAK_STATE, NOMINAL_STATE } sensorState_t;

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void printStatus();
static void printStatusDef();
static void updateBoardStatus();
static void userInput(const char *input);
static float voltageToResistance(float volt);
static void adcToFloat(int16_t *pData);
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);
static void toggleBoostPin();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

// Buffer shared by printStatus and printStatusDef
static char printBuffer[300];

static float sensorVoltages[NO_OF_SENSORS]    = {0.0};  // V    - Sense voltages of sensors
static float sensorResistances[NO_OF_SENSORS] = {0.0};  // kOhm - Resistances of sensors
static float voltageBoost                     = 0.0;    // V    - Output voltage applied to sensors
static float voltageVCC                       = 0.0;    // V    - VCC voltage

// Boost controller
static struct {
    int boostOnTime;
    int boostOffTime;
    unsigned long timeStamp;
    bool inSwitchBoostMode;
    bool isOn;
} boostController = {0, 0, 0, false, false};

static StmGpio BoostEn;                // GPIO to activate boost converter
TIM_HandleTypeDef *boostTimer = NULL;  // Timer triggering toggling of boost voltage
static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];  // ADC circular buffer

static CAProtocolCtx caProto = {.undefined        = userInput,
                                .printHeader      = CAPrintHeader,
                                .printStatus      = printStatus,
                                .printStatusDef   = printStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = NULL,
                                .calibrationRW    = NULL,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief   Definition of status information when the 'Status' command is received
 */
static void printStatus() {
    int len = 0;

    if (bsGetField(BS_BOOST_ERROR_Msk)) {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                        "Boost error: YES | Boost voltage: %0.2fV\r\n", voltageBoost);
    }
    else {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "Boost error: NO\r\n");
    }

    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "Boost mode active: %s\r\n",
                    bsGetField(BS_BOOST_ACTIVE_Msk) ? "YES" : "NO");
    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "Boost mode pin: %s\r\n",
                    bsGetField(BS_BOOST_PIN_HIGH_Msk) ? "ON" : "OFF");

    bool isSensing = !bsGetField(BS_BOOST_ERROR_Msk) && bsGetField(BS_BOOST_PIN_HIGH_Msk) &&
                     !bsGetField(BS_UNDER_VOLTAGE_Msk);

    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                        "Sensor %d state: ", sensorId + 1);
        if (isSensing) {
            switch ((bsGetStatus() & BS_SENSOR_STATE_RANGE_Msk(sensorId)) >>
                    BS_SENSOR_STATE_RANGE_Pos(sensorId)) {
                case NC_OR_BROKEN_STATE:
                    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "NC/BROKEN\r\n");
                    break;
                case BROKEN_STATE:
                    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "BROKEN\r\n");
                    break;
                case LEAK_STATE:
                    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "LEAK\r\n");
                    break;
                case NOMINAL_STATE:
                    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "NOMINAL\r\n");
                    break;
                default:
                    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "ERROR\r\n");
                    break;
            }
        }
        else {
            len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "NOT SENSING\r\n");
        }
    }

    writeUSB(printBuffer, len);
}

/*!
 * @brief   Definition of status information definition when the 'StatusDef' command is received
 */
static void printStatusDef() {
    int len = 0;

    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                    "0x%08" PRIx32 ",Boost error error\r\n", (uint32_t)BS_BOOST_ERROR_Msk);
    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                    "0x%08" PRIx32 ",Boost mode active\r\n", (uint32_t)BS_BOOST_ACTIVE_Msk);
    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                    "0x%08" PRIx32 ",Boost mode pin\r\n", (uint32_t)BS_BOOST_PIN_HIGH_Msk);

    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                        "0x%08" PRIx32 ",Sensor %d state\r\n",
                        (uint32_t)BS_SENSOR_STATE_RANGE_Msk(sensorId), sensorId + 1);
    }

    writeUSB(printBuffer, len);
}

/*!
 * @brief Updates the board status
 */
static void updateBoardStatus() {
    static const float BOOST_OK_DIFF = 0.2;  // V  - Accepted boost voltage difference
    static const float VCC_OK_DIFF   = 0.3;  // V  - Accepted VCC voltage difference

    static const float BROKEN_NC_LIM      = 0.8;   // V
    static const float BROKEN_OK_LOW_LIM  = 6.95;  // V
    static const float BROKEN_OK_HIGH_LIM = 35.7;  // V

    // Error bits
    bsUpdateError(BS_BOOST_ERROR_Msk, fabsf(voltageBoost - V_BOOST_NOMINAL) > BOOST_OK_DIFF,
                  SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_UNDER_VOLTAGE_Msk, voltageVCC < VCC_NOMINAL - VCC_OK_DIFF,
                  SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_OVER_VOLTAGE_Msk, voltageVCC > VCC_NOMINAL + VCC_OK_DIFF, SALTLEAK_ERRORS_Msk);

    // Field bits
    bsUpdateField(BS_BOOST_ACTIVE_Msk, boostController.inSwitchBoostMode);
    bsUpdateField(BS_BOOST_PIN_HIGH_Msk, stmGetGpio(BoostEn));

    bool isSensing = !bsGetField(BS_BOOST_ERROR_Msk) && bsGetField(BS_BOOST_PIN_HIGH_Msk) &&
                     !bsGetField(BS_UNDER_VOLTAGE_Msk);

    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        sensorState_t state = NOMINAL_STATE;
        if (isSensing) {
            if (sensorVoltages[sensorId] < BROKEN_NC_LIM) {
                state = NC_OR_BROKEN_STATE;
            }
            else if (sensorVoltages[sensorId] < BROKEN_OK_LOW_LIM ||
                     sensorVoltages[sensorId] > BROKEN_OK_HIGH_LIM) {
                state = BROKEN_STATE;
            }
            else if (sensorResistances[sensorId] < LEAK_THRESHOLD) {
                state = LEAK_STATE;
            }
        }
        bsSetFieldRange(state << BS_SENSOR_STATE_RANGE_Pos(sensorId),
                        BS_SENSOR_STATE_RANGE_Msk(sensorId));
    }
}

/*!
 * @brief   Implementation of the user input
 * @param   input Pointer to the message received
 */
static void userInput(const char *input) {
    static int onTime;
    static int offTime;

    if (sscanf(input, "boost %d %d", &onTime, &offTime) == 2) {
        // Input in seconds, but comparison in milliseconds
        boostController.boostOnTime       = onTime * 1000;
        boostController.boostOffTime      = offTime * 1000;
        boostController.inSwitchBoostMode = true;
    }
    else if (strncmp(input, "switch off", 10) == 0) {
        boostController.inSwitchBoostMode = false;
    }
    else {
        HALundefined(input);
    }
}

/*!
 * @brief   Estimates the resistance between the electrodes based on the sense voltage
 * @param   volt Measured voltage
 * @return  Resistance in kOhm
 */
static float voltageToResistance(float volt) {
    static const float RES_P1 = 6.2;    // kOhm  - Resistor on P1 sensor leg
    static const float RES_P2 = 100.0;  // kOhm  - Resistor on P2 sensor leg
    static const float RES_N1 = 18.0;   // kOhm  - Resistor on N1 sensor leg

    float num = RES_P2 * (RES_P1 + RES_N1 - (RES_N1 * V_BOOST_NOMINAL / volt));
    float den = RES_N1 * V_BOOST_NOMINAL / volt - (RES_P1 + RES_P2 + RES_N1);

    return den != 0.0 ? num / den : 10e3;
}

/*!
 * @brief   Conversion of ADC values into physical values
 * @param   pData Pointer to the latest ADC values
 */
static void adcToFloat(int16_t *pData) {
    // From voltage divider on PCB - in V
    static const float SENSE_VOLT_SCALAR =
        ANALOG_REF_VOLTAGE * (3.9e6 + 270e3) / 270e3 / (ADC_MAX + 1);

    // From voltage divider on PCB - in V
    static const float BOOST_SCALAR = ANALOG_REF_VOLTAGE * (3.9e6 + 270e3) / 270e3 / (ADC_MAX + 1);

    // From voltage divider on PCB - in V
    static const float VCC_SCALAR = ANALOG_REF_VOLTAGE * (15e3 + 21.5e3) / 21.5e3 / (ADC_MAX + 1);

    // Sense voltages and estimated resistances
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        sensorVoltages[sensorId]    = ADCMean(pData, sensorId) * SENSE_VOLT_SCALAR;
        sensorResistances[sensorId] = voltageToResistance(sensorVoltages[sensorId]);
    }

    // Voltage feedbacks
    voltageBoost = ADCMean(pData, 6) * BOOST_SCALAR;
    voltageVCC   = ADCMean(pData, 7) * VCC_SCALAR;

    setBoardVoltage(voltageVCC);
}

/*!
 * @brief   Callback function for the ADC (every 0.1s)
 * @param   pData Pointer to the latest ADC values
 * @param   noOfChannels Number of ADC channels
 * @param   noOfSamples Number of samples
 */
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples) {
    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        if (isUsbPortOpen()) {
            USBnprintf("0x%08" PRIx32, bsGetStatus());
        }
        return;
    }

    adcToFloat(pData);
    updateBoardStatus();

    char buf[150] = {0};
    int len       = 0;

    // Estimated sensor resistances
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        len += sprintf(&buf[len], "%.2f, ", sensorResistances[sensorId]);
    }

    // Leak detection
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        bool isLeak = (bsGetStatus() & BS_SENSOR_STATE_RANGE_Msk(sensorId)) >>
                          BS_SENSOR_STATE_RANGE_Pos(sensorId) ==
                      LEAK_STATE;
        len += sprintf(&buf[len], "%d, ", isLeak);
    }

    // Board status
    len += sprintf(&buf[len], "%0.2f, %0.2f, 0x%08" PRIx32 "\r\n", voltageVCC, voltageBoost,
                   bsGetStatus());

    if (isUsbPortOpen()) {
        writeUSB(buf, len);
    }
}

/*!
 * @brief   Turn on and off boost module after user defined time interval
 */
static void toggleBoostPin() {
    bool switchControl = (boostController.isOn) ? false : true;
    int switchTime =
        (boostController.isOn) ? boostController.boostOnTime : boostController.boostOffTime;

    if (HAL_GetTick() - boostController.timeStamp >= switchTime) {
        boostController.isOn      = switchControl;
        boostController.timeStamp = HAL_GetTick();
        stmSetGpio(BoostEn, switchControl);
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief   Overwritten timer callback function which toogle the boost pin
 * @param   htim Pointer to the involved timer handler
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    // loopTimer corresponds to htim5 which triggers with a frequency of 10Hz
    if (htim != boostTimer) {
        return;
    }

    if (!boostController.inSwitchBoostMode) {
        return;
    }

    toggleBoostPin();
}

/*!
 * @brief   Initialization function called one time after start-up
 * @param   hadc1 Pointer to the ADC handler
 * @param   htim5 Pointer to the timer handler
 */
void saltleakInit(ADC_HandleTypeDef *hadc1, TIM_HandleTypeDef *htim5) {
    initCAProtocol(&caProto, usbRx);

    // Don't allow NULL handles to float about if its the wrong board type
    boostTimer = htim5;

    // ADC must be initialised for USB printout to work. Doesn't matter if the board type is wrong
    ADCMonitorInit(hadc1, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));

    if (boardSetup(SaltLeak, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, SALTLEAK_ERRORS_Msk) !=
        0) {
        return;
    }

    // GPIO
    stmGpioInit(&BoostEn, BOOST_EN_GPIO_Port, BOOST_EN_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(BoostEn, true);

    // Timer
    HAL_TIM_Base_Start_IT(boostTimer);
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void saltleakLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(adcCallback);
}
