/*!
 * @file    saltleakLoop.c
 * @brief   This file contains the main program of Saltleak
 * @date    11/11/2021
 * @authors agp, Timothé Dodin
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
#include "calibration.h"
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

#define NO_OF_SENSORS  6     // Number of salt leak sensors
#define LEAK_THRESHOLD 10.0  // kOhm  - Leak threshold (to be tuned)

#define V_BOOST_NOMINAL 48.0  // V  - Nominal boost voltage
#define V_BOOST_MIN     45.0  // V  - Minimum boost voltage
#define V_BOOST_MAX     51.0  // V  - Maximum boost voltage

#define VCC_NOMINAL 5.0  // V   - Nominal USB voltage after isolator
#define VCC_MIN     4.8  // V   - Minimum USB voltage after isolator
#define VCC_MAX     5.2  // V   - Maximum USB voltage after isolator

#define BROKEN_NC_LIM      (0.8 / V_BOOST_NOMINAL)   // BROKEN/NC voltage ratio threshold
#define BROKEN_OK_LOW_LIM  (6.95 / V_BOOST_NOMINAL)  // BROKEN/OK low voltage ratio threshold
#define BROKEN_OK_HIGH_LIM (35.7 / V_BOOST_NOMINAL)  // BROKEN/OK high voltage ratio threshold

/* Specific Board Status register definitions */

// Boost voltage out of nominal value
#define BS_BOOST_ERROR_Pos 14U
#define BS_BOOST_ERROR_Msk (1UL << BS_BOOST_ERROR_Pos)

// Boost controller active
#define BS_BOOST_SWITCH_ACTIVE_Pos 13U
#define BS_BOOST_SWITCH_ACTIVE_Msk (1UL << BS_BOOST_SWITCH_ACTIVE_Pos)

// Boost converter enabled
#define BS_BOOST_PIN_HIGH_Pos 12U
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

static void saltLeakPrintHeader();
static void saltLeakPrintStatus();
static void saltLeakPrintStatusDef();
static void saltLeakCalibration(int noOfCalibrations, const CACalibration *calibrations);
static void saltLeakCalibrationRW(bool write);
static void saltLeakLogging(int port);

static void userInput(const char *input);
static void updateBoardStatus();

static void voltageToResistance();
static void adcToFloat(int16_t *pData);
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);

static void toggleBoostPin();
static void updateBoostMode();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

// Buffer shared by printStatus and printStatusDef
static char printBuffer[300];

static float sensorVoltages[NO_OF_SENSORS]    = {0.0};  // V    - Sense voltages of sensors
static float sensorResistances[NO_OF_SENSORS] = {0.0};  // kOhm - Resistances of sensors
static float voltageBoost                     = 0.0;    // V    - Output voltage applied to sensors
static float voltageVCC                       = 0.0;    // V    - VCC voltage

// To print voltages or resistances
static bool printVoltages = false;

// Boost controller
static struct {
    uint32_t boostOnTime;
    uint32_t boostOffTime;
    uint32_t timeStamp;
    bool inSwitchBoostMode;
    bool isOn;
} boostController = {0, 0, 0, false, false};

// GPIO to activate boost converter
static StmGpio BoostEn;

// ADC circular buffer
static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];

// Calibration
static FlashCalibration_t cal;

// CA protocol handling
static CAProtocolCtx caProto = {.undefined        = userInput,
                                .printHeader      = saltLeakPrintHeader,
                                .printStatus      = saltLeakPrintStatus,
                                .printStatusDef   = saltLeakPrintStatusDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = saltLeakCalibration,
                                .calibrationRW    = saltLeakCalibrationRW,
                                .logging          = saltLeakLogging,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL};

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Definition of what is printed when the 'Serial' command is received
 */
static void saltLeakPrintHeader() {
    CAPrintHeader();
    saltLeakCalibrationRW(false);
}

/*!
 * @brief   Definition of status information when the 'Status' command is received
 */
static void saltLeakPrintStatus() {
    int len = 0;

    if (bsGetField(BS_BOOST_ERROR_Msk)) {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                        "Boost error: YES | Boost voltage: %0.2fV\r\n", voltageBoost);
    }
    else {
        len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "Boost error: NO\r\n");
    }

    len +=
        snprintf(&printBuffer[len], sizeof(printBuffer) - len, "Boost switch mode active: %s\r\n",
                 bsGetField(BS_BOOST_SWITCH_ACTIVE_Msk) ? "YES" : "NO");
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
static void saltLeakPrintStatusDef() {
    int len = 0;

    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len, "0x%08" PRIx32 ",Boost error\r\n",
                    (uint32_t)BS_BOOST_ERROR_Msk);
    len += snprintf(&printBuffer[len], sizeof(printBuffer) - len,
                    "0x%08" PRIx32 ",Boost switch mode active\r\n",
                    (uint32_t)BS_BOOST_SWITCH_ACTIVE_Msk);
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
 * @brief Applies the calibration sent by the user
 * @param noOfCalibrations Number of calibrations
 * @param calibrations Pointer to the calibration structure
 */
static void saltLeakCalibration(int noOfCalibrations, const CACalibration *calibrations) {
    calibration(noOfCalibrations, calibrations, &cal, sizeof(cal), sensorVoltages, voltageBoost);
}

/*!
 * @brief Printing of the calibration information when the 'Serial' command is received
 * @param write Write directly into the flash memory
 */
static void saltLeakCalibrationRW(bool write) { calibrationRW(write, &cal, sizeof(cal)); }

/*!
 * @brief The user selects voltages or resistances to be printed
 * @param port p0 or p1
 */
static void saltLeakLogging(int port) {
    if (port == 0) {
        printVoltages = false;
    }
    else if (port == 1) {
        printVoltages = true;
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
 * @brief Updates the board status
 */
static void updateBoardStatus() {
    // Field bits
    bsUpdateField(BS_BOOST_SWITCH_ACTIVE_Msk, boostController.inSwitchBoostMode);
    bsUpdateField(BS_BOOST_PIN_HIGH_Msk, stmGetGpio(BoostEn));

    // Error bits
    bsUpdateError(BS_BOOST_ERROR_Msk,
                  bsGetField(BS_BOOST_PIN_HIGH_Msk) &&
                      (voltageBoost > V_BOOST_MAX || voltageBoost < V_BOOST_MIN),
                  SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_UNDER_VOLTAGE_Msk, voltageVCC < VCC_MIN, SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_OVER_VOLTAGE_Msk, voltageVCC > VCC_MAX, SALTLEAK_ERRORS_Msk);

    // Range fields
    bool isSensing = !bsGetField(BS_BOOST_ERROR_Msk) && bsGetField(BS_BOOST_PIN_HIGH_Msk) &&
                     !bsGetField(BS_UNDER_VOLTAGE_Msk);

    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        sensorState_t state = NOMINAL_STATE;
        if (isSensing) {
            if (sensorVoltages[sensorId] < BROKEN_NC_LIM * voltageBoost) {
                state = NC_OR_BROKEN_STATE;
            }
            else if (sensorVoltages[sensorId] < BROKEN_OK_LOW_LIM * voltageBoost ||
                     sensorVoltages[sensorId] > BROKEN_OK_HIGH_LIM * voltageBoost) {
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
 * @brief   Estimates the resistance between the electrodes based on the sense voltage
 */
static void voltageToResistance() {
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        if (sensorVoltages[sensorId] == 0.0 || voltageBoost > V_BOOST_MAX ||
            voltageBoost < V_BOOST_MIN) {
            sensorResistances[sensorId] = -1.0;  // Incorrect resistance value
            continue;
        }

        // Formulas derived from resistors network
        float num = cal.sensorCal[sensorId].resP2 *
                    (cal.sensorCal[sensorId].resP1 + cal.sensorCal[sensorId].resN1 -
                     (cal.sensorCal[sensorId].resN1 * voltageBoost / sensorVoltages[sensorId]));
        float den = cal.sensorCal[sensorId].resN1 * voltageBoost / sensorVoltages[sensorId] -
                    (cal.sensorCal[sensorId].resP1 + cal.sensorCal[sensorId].resP2 +
                     cal.sensorCal[sensorId].resN1);

        if (den == 0.0) {
            sensorResistances[sensorId] = -1.0;  // Incorrect resistance value
            continue;
        }

        float res = num / den;

        // Resistance <= 0 means that the sensor is NC/broken
        sensorResistances[sensorId] = res > 0.0 ? res : -1.0;
    }
}

/*!
 * @brief   Conversion of ADC values into physical values
 * @param   pData Pointer to the latest ADC values
 */
static void adcToFloat(int16_t *pData) {
    // From voltage divider on PCB - in V
    static const float VCC_SCALAR = ANALOG_REF_VOLTAGE * (15e3 + 21.5e3) / 21.5e3 / (ADC_MAX + 1);

    // Voltage feedbacks
    voltageBoost = ADCMean(pData, 6) * cal.boostScalar1000 * 1e-3;
    voltageVCC   = ADCMean(pData, 7) * VCC_SCALAR;
    setBoardVoltage(voltageVCC);

    // Sense voltages
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        sensorVoltages[sensorId] =
            ADCMean(pData, sensorId) * cal.sensorCal[sensorId].vScalar1000 * 1e-3;
    }

    // Resistance estimation
    voltageToResistance();
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
    updateBoostMode();

    char buf[150] = {0};
    int len       = snprintf(buf, 3, "\r\n");

    // Estimated sensor resistances/voltages
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        if (printVoltages) {
            len += sprintf(&buf[len], "%0.2f, ", sensorVoltages[sensorId]);
        }
        else {
            len += sprintf(&buf[len], "%0.2f, ", sensorResistances[sensorId]);
        }
    }

    // Leak detection
    for (uint8_t sensorId = 0; sensorId < NO_OF_SENSORS; sensorId++) {
        bool isLeaking = (bsGetStatus() & BS_SENSOR_STATE_RANGE_Msk(sensorId)) >>
                             BS_SENSOR_STATE_RANGE_Pos(sensorId) ==
                         LEAK_STATE;
        len += sprintf(&buf[len], "%d, ", isLeaking);
    }

    // Boost voltage and board status
    len += sprintf(&buf[len], "%0.2f, 0x%08" PRIx32, voltageBoost, bsGetStatus());

    if (isUsbPortOpen()) {
        writeUSB(buf, len);
    }
}

/*!
 * @brief   Turn on and off boost module after user defined time interval
 */
static void toggleBoostPin() {
    bool switchControl = (boostController.isOn) ? false : true;
    uint32_t switchTime =
        (boostController.isOn) ? boostController.boostOnTime : boostController.boostOffTime;

    if (HAL_GetTick() - boostController.timeStamp >= switchTime) {
        boostController.isOn      = switchControl;
        boostController.timeStamp = HAL_GetTick();
        stmSetGpio(BoostEn, switchControl);
    }
}

/*!
 * @brief   Update boost mode according to user decision
 */
static void updateBoostMode() {
    if (boostController.inSwitchBoostMode) {
        toggleBoostPin();
    }
    else {
        boostController.isOn = true;
        stmSetGpio(BoostEn, true);
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief   Initialization function called one time after start-up
 * @param   hadc1 Pointer to the ADC handler
 * @param   htim5 Pointer to the timer handler
 */
void saltleakInit(ADC_HandleTypeDef *hadc1, CRC_HandleTypeDef *hcrc) {
    initCAProtocol(&caProto, usbRx);

    // ADC must be initialised for USB printout to work. Doesn't matter if the board type is wrong
    ADCMonitorInit(hadc1, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));

    if (boardSetup(SaltLeak, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, SALTLEAK_ERRORS_Msk) !=
        0) {
        return;
    }

    // Calibration
    calibrationInit(hcrc, &cal, sizeof(cal));

    // GPIO
    stmGpioInit(&BoostEn, BOOST_EN_GPIO_Port, BOOST_EN_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(BoostEn, true);  // Activates boost converter by default
}

/*!
 * @brief   Function called in the main function
 * @param   bootMsg Pointer to the boot message defined in CA library
 */
void saltleakLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    ADCMonitorLoop(adcCallback);
}
