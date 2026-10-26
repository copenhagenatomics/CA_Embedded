/*!
 * @file    humidityApp.c
 * @brief   This file contains the main program of Humidity
 * @date    06/04/2022
 * @author  matias
 */

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "HAL_otp.h"
#include "USBprint.h"
#include "githash.h"
#include "humidityApp.h"
#include "humiditySm.h"
#include "pcbversion.h"
#include "systemInfo.h"
#include "time32.h"
#include "uptime.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define NUM_SENSORS 2

/***************************************************************************************************
** PRIVATE FUNCTION PROTOTYPES
***************************************************************************************************/

static void HumidityPrintStatus();
static void HumidityPrintStatusDef();
static void HumidityPrintOutputDef();
static void userInputs(const char* input);
static void humidityUptimeHandler(const char* input);

static void initHumiditySensors(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static humidity_sm_t humiditySensors[NUM_SENSORS] = {0};

// Print buffer shared
static char buf[600];

static CAProtocolCtx caProto = {.undefined        = userInputs,
                                .printHeader      = CAPrintHeader,
                                .printStatus      = HumidityPrintStatus,
                                .printStatusDef   = HumidityPrintStatusDef,
                                .printOutputDef   = HumidityPrintOutputDef,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration      = NULL,
                                .calibrationRW    = NULL,
                                .logging          = NULL,
                                .otpRead          = CAotpRead,
                                .otpWrite         = NULL,
                                .uptime           = humidityUptimeHandler};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
 * @brief Verbose print of the Humidity board status.
 */
static void HumidityPrintStatus() {
    int len = 0;

    for (uint16_t i = 0; i < NUM_SENSORS; i++) {
        CA_SNPRINTF(buf, len, "Serial number sensor %u: 0x%08" PRIx32 ".\r\n", i + 1,
                    humidity_sm_get_serial(&humiditySensors[i]));
    }

    for (uint16_t i = 0; i < NUM_SENSORS; i++) {
        CA_SNPRINTF(buf, len, "Humidity sensor %u is %s.\r\n", i + 1,
                    bsGetField(SHT45_ERROR_Msk(i)) ? "not responding" : "operating normally");
    }

    writeUSB(buf, len);
}

/*!
 * @brief Definition of status information when the 'StatusDef' command is received
 */
static void HumidityPrintStatusDef() {
    int len = 0;

    for (uint16_t i = 0; i < NUM_SENSORS; i++) {
        CA_SNPRINTF(buf, len, "0x%08" PRIx32 ",Humidity sensor %u error\r\n",
                    (uint32_t)SHT45_ERROR_Msk(i), i + 1);
    }

    writeUSB(buf, len);
}

/*!
 * @brief Definition of output printed when the 'OutputDef' command is received
 */
static void HumidityPrintOutputDef() {
    int len = 0;

    for (uint16_t i = 1; i <= NUM_SENSORS; i++) {
        CA_SNPRINTF(buf, len, "Temperature %u,degC\r\n", i);
        CA_SNPRINTF(buf, len, "Relative humidity %u,pct\r\n", i);
        CA_SNPRINTF(buf, len, "Absolute humidity %u,g/m3\r\n", i);
    }

    writeUSB(buf, len);
}

/*!
 * @brief User input handler that allows to go into burn-in mode.
 * @param input User input buffer
 */
static void userInputs(const char* input) {
    if (strncmp(input, "burnin", 6) == 0) {
        for (uint16_t i = 0; i < NUM_SENSORS; i++) {
            humidity_sm_start_burnin(&humiditySensors[i]);
        }
    }
    else if (strncmp(input, "normal", 6) == 0) {
        for (uint16_t i = 0; i < NUM_SENSORS; i++) {
            humidity_sm_stop_burnin(&humiditySensors[i]);
        }
    }
    else {
        HALundefined(input);
    }
}

/*!
 * @brief Handles the "uptime" command
 * @param input User input buffer
 */
static void humidityUptimeHandler(const char* input) {
    uptime_inputHandler(input, CAPrintHeader);
}

/*!
 *  @brief State machine for humidity measurements.
 */
static void humidityStateMachine() {
    for (uint16_t i = 0; i < NUM_SENSORS; i++) {
        humidity_sm_run(&humiditySensors[i]);
    }

    bsClearError(HUMIDITY_NO_ERROR_Msk);
}

/*!
 * @brief Assigns I2C handles to sensor handles and tries to read the SHT45 serial
 *        information to determine whether it has a valid connection.
 * @param hi2c1 First I2C
 * @param hi2c2 Second I2C
 */
static void initHumiditySensors(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2) {
    humidity_sm_init(&humiditySensors[0], hi2c1, GPIOB, GPIO_PIN_6, GPIOB, GPIO_PIN_7,
                     SHT45_ERROR_Msk(0));
    humidity_sm_init(&humiditySensors[1], hi2c2, GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_3,
                     SHT45_ERROR_Msk(1));
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
 * @brief Timer interrupt with an interrupt frequency of 10 Hz. The function is responsible for
 *        printing measurement data and board status over USB.
 * @param htim Timer calling the interrupt
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) {
    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        USBnprintf("0x%08" PRIx32 "\r\n", bsGetStatus());
        return;
    }

    int len = 0;

    for (uint16_t i = 0; i < NUM_SENSORS; i++) {
        CA_SNPRINTF(buf, len, "%.2f, %.2f, %.2f, ", humidity_sm_get_temp(&humiditySensors[i]),
                    humidity_sm_get_rh(&humiditySensors[i]),
                    humidity_sm_get_ah(&humiditySensors[i]));
    }
    CA_SNPRINTF(buf, len, "0x%08" PRIx32 "\r\n", bsGetStatus());

    writeUSB(buf, len);
}

/*!
 * @brief Loop function called repeatedly in main loop
 *
 * * Responds to user input
 * * Initiates measurements, waits in a non-blocking manner for conversions
 *   to finish and gets new measurements depending on current sensor state.
 */
void LoopHumidity(const char* bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    uptime_update();

    if (bsGetField(BS_VERSION_ERROR_Msk)) {
        return;
    }

    humidityStateMachine();
}

/*!
 * @brief Setup function called at startup
 * @param hi2c1 First I2C handler
 * @param hi2c2 Second I2C handler
 * @param hcrc CRC Handler
 * @param bootMsg CA boot message
 */
void InitHumidity(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2, CRC_HandleTypeDef* hcrc,
                  const char* bootMsg) {
    initCAProtocol(&caProto, usbRx);

    // Initialises basic uptime counters
    uptime_init(hcrc, 0, NULL, bootMsg, GIT_VERSION);

    // PCB V1.6 of the humidity board uses SHT45 i.e. not functional on older PCB versions.
    if (boardSetup(HumidityChip, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR},
                   HUMIDITY_NO_ERROR_Msk) != 0) {
        return;
    }

    initHumiditySensors(hi2c1, hi2c2);
}
