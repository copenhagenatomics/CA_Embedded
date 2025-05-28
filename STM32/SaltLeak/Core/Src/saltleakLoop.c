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

#define ADC_CHANNELS          8     // Number of ADC channels used on the STM32
#define ADC_CHANNEL_BUF_SIZE 400   // 4 kHz sampling  rate -> 10 Hz
#define ADC_MAX               4095  // 12-bits
#define ANALOG_REF_VOLTAGE    3.3   // V

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

typedef enum {
    NOMINAL_STATE = 0,
    LEAK_STATE,
    BROKEN_STATE,
    NC_OR_BROKEN_STATE,
    BOOST_ERROR_STATE,
    INACTIVE_STATE
} sensorState_t;

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
static void updateSensorStates();

static void voltageToResistance();
static void adcToFloat(int16_t *pData);
static void adcCallback(int16_t *pData, int noOfChannels, int noOfSamples);

static void toggleBoostPin();
static void updateBoostMode();

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

// Buffer shared by printStatus and printStatusDef
static char buff[300];

static sensorState_t sensorStates[NO_OF_SENSORS] = {NOMINAL_STATE};  // Sensor states
static float sensorVoltages[NO_OF_SENSORS]       = {0.0};  // V    - Sense voltages of sensors
static float sensorResistances[NO_OF_SENSORS]    = {0.0};  // kOhm - Resistances of sensors
static float voltageBoost                        = 0.0;  // V    - Output voltage applied to sensors
static float voltageVCC                          = 0.0;  // V    - VCC voltage

// To print voltages or resistances
static bool printVoltages = false;

// Boost controller
static struct {
    uint32_t boostOnTime;
    uint32_t boostOffTime;
    uint32_t timeStamp;
    bool inSwitchBoostMode;
} boostController = {0, 0, 0, false};

// GPIO to activate boost converter
static StmGpio BoostEn;

// ADC circular buffer
static int16_t ADCbuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];

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
        CA_SNPRINTF(buff, len, "Boost error: YES | Boost voltage: %0.2fV\r\n", voltageBoost);
    }
    else {
        CA_SNPRINTF(buff, len, "Boost error: NO\r\n");
    }

    CA_SNPRINTF(buff, len, "Boost switch: %s\r\n", bsGetField(BS_BOOST_SWITCH_Msk) ? "YES" : "NO");
    CA_SNPRINTF(buff, len, "Boost pin: %s\r\n", bsGetField(BS_BOOST_PIN_Msk) ? "ON" : "OFF");

    writeUSB(buff, len);
}

/*!
 * @brief   Definition of status information definition when the 'StatusDef' command is received
 */
static void saltLeakPrintStatusDef() {
    int len = 0;

    CA_SNPRINTF(buff, len, "0x%08" PRIx32 ",Boost error\r\n", (uint32_t)BS_BOOST_ERROR_Msk);
    CA_SNPRINTF(buff, len, "0x%08" PRIx32 ",Switch mode\r\n", (uint32_t)BS_BOOST_SWITCH_Msk);
    CA_SNPRINTF(buff, len, "0x%08" PRIx32 ",Boost pin\r\n", (uint32_t)BS_BOOST_PIN_Msk);

    writeUSB(buff, len);
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
    bsUpdateField(BS_BOOST_SWITCH_Msk, boostController.inSwitchBoostMode);
    bsUpdateField(BS_BOOST_PIN_Msk, stmGetGpio(BoostEn));

    // Error bits
    bsUpdateError(
        BS_BOOST_ERROR_Msk,
        bsGetField(BS_BOOST_PIN_Msk) && (voltageBoost > V_BOOST_MAX || voltageBoost < V_BOOST_MIN),
        SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_UNDER_VOLTAGE_Msk, voltageVCC < VCC_MIN, SALTLEAK_ERRORS_Msk);
    bsUpdateError(BS_OVER_VOLTAGE_Msk, voltageVCC > VCC_MAX, SALTLEAK_ERRORS_Msk);
}

/*!
 * @brief Updates the sensor states
 */
static void updateSensorStates() {
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        if (!bsGetField(BS_BOOST_PIN_Msk)) {
            sensorStates[i] = INACTIVE_STATE;
        }
        else if (bsGetField(BS_BOOST_ERROR_Msk)) {
            sensorStates[i] = BOOST_ERROR_STATE;
        }
        else if (sensorVoltages[i] < BROKEN_NC_LIM * voltageBoost) {
            sensorStates[i] = NC_OR_BROKEN_STATE;
        }
        else if (sensorVoltages[i] < BROKEN_OK_LOW_LIM * voltageBoost ||
                 sensorVoltages[i] > BROKEN_OK_HIGH_LIM * voltageBoost) {
            sensorStates[i] = BROKEN_STATE;
        }
        else if (sensorResistances[i] < LEAK_THRESHOLD) {
            sensorStates[i] = LEAK_STATE;
        }
        else {
            sensorStates[i] = NOMINAL_STATE;
        }
    }
}

/*!
 * @brief   Estimates the resistance between the electrodes based on the sense voltage
 */
static void voltageToResistance() {
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        sensorCalibration_t *sc = &cal.sensorCal[i];

        if (sensorVoltages[i] == 0.0) {
            sensorResistances[i] = -1.0;  // Incorrect resistance value
            continue;
        }

        // Formulas derived from resistors network
        float num =
            sc->resP2 * (sc->resP1 + sc->resN1 - (sc->resN1 * voltageBoost / sensorVoltages[i]));
        float den =
            sc->resN1 * voltageBoost / sensorVoltages[i] - (sc->resP1 + sc->resP2 + sc->resN1);

        if (den == 0.0) {
            sensorResistances[i] = 1e6;
            continue;
        }

        // Resistance <= 0 means that the sensor is NC/broken
        sensorResistances[i] = num / den;
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
    voltageBoost = ADCMean(pData, 6) * cal.boostScalar;
    voltageVCC   = ADCMean(pData, 7) * VCC_SCALAR;
    setBoardVoltage(voltageVCC);

    // Sense voltages
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        sensorVoltages[i] = ADCMean(pData, i) * cal.sensorCal[i].vScalar;
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
    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    adcToFloat(pData);
    updateBoardStatus();
    updateSensorStates();
    updateBoostMode();

    int len = 0;
    CA_SNPRINTF(buff, len, "\r\n");

    // Estimated sensor resistances/voltages
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        if (printVoltages) {
            CA_SNPRINTF(buff, len, "%0.2f, ", sensorVoltages[i]);
        }
        else {
            CA_SNPRINTF(buff, len, "%0.2f, ", sensorResistances[i]);
        }
    }

    // Sensor states
    for (uint8_t i = 0; i < NO_OF_SENSORS; i++) {
        CA_SNPRINTF(buff, len, "%d, ", sensorStates[i]);
    }

    // Boost voltage and board status
    CA_SNPRINTF(buff, len, "%0.2f, 0x%08" PRIx32, voltageBoost, bsGetStatus());

    writeUSB(buff, len);
}

/*!
 * @brief   Turn on and off boost module after user defined time interval
 */
static void toggleBoostPin() {
    bool switchControl = stmGetGpio(BoostEn) ? false : true;
    uint32_t switchTime =
        stmGetGpio(BoostEn) ? boostController.boostOnTime : boostController.boostOffTime;

    if (HAL_GetTick() - boostController.timeStamp >= switchTime) {
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
    ADCMonitorInit(hadc1, ADCbuffer, sizeof(ADCbuffer) / sizeof(int16_t));

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
