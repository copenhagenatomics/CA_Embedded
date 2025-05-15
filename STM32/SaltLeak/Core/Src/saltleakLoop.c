/*
 * saltleakLoop.c
 *
 *  Created on: Nov 11, 2021
 *      Author: agp
 *
 *  Description: Drives the application part of the Saltleak detection.
 */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#include "ADCMonitor.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "StmGpio.h"
#include "USBprint.h"
#include "main.h"
#include "pcbversion.h"
#include "saltleakLoop.h"
#include "systemInfo.h"

#define ADC_CHANNELS 10
#define ADC_CHANNEL_BUF_SIZE 400
#define LEAK_THRESHOLD 1500

static StmGpio BoostEn;

// Boost controller
static struct {
    int boostOnTime;
    int boostOffTime;
    unsigned long timeStamp;
    bool inSwitchBoostMode;
    bool isOn;
} boostController = {0, 0, 0, false, false};

static int16_t ADCBuffer[ADC_CHANNELS * ADC_CHANNEL_BUF_SIZE * 2];

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void userInput(const char *input);
static void updateBoardStatus();
static void printStatus();

static CAProtocolCtx caProto = {.undefined = userInput,
                                .printHeader = CAPrintHeader,
                                .printStatus = printStatus,
                                .jumpToBootLoader = HALJumpToBootloader,
                                .calibration = NULL,
                                .calibrationRW = NULL,
                                .logging = NULL,
                                .otpRead = CAotpRead,
                                .otpWrite = NULL};

/*!
** @brief Verbose prints the board status
*/
static void printStatus() {
    static char buf[600] = {0};
    int len = 0;

    len += snprintf(&buf[len], sizeof(buf) - len, "Boost mode active: %d\r\n",
                    boostController.inSwitchBoostMode);
    len += snprintf(&buf[len], sizeof(buf) - len, "Boost mode pin: %d\r\n", boostController.isOn);

    writeUSB(buf, len);
}

/*!
** @brief Updates the board status
*/
static void updateBoardStatus() {
    boostController.inSwitchBoostMode ? bsSetField(BOOST_ACTIVE_Msk)
                                      : bsClearField(BOOST_ACTIVE_Msk);

    // if (boostGpio.get != NULL) {
    //     stmGetGpio(boostGpio) ? bsSetField(BOOST_PIN_HIGH_Msk) : bsClearField(BOOST_PIN_HIGH_Msk);
    // }
    // else {
    //     bsClearField(BOOST_PIN_HIGH_Msk);
    // }

    bsClearError(SALTLEAK_NO_ERROR_Msk);
}

static void userInput(const char *input) {
    static int onTime;
    static int offTime;

    if (sscanf(input, "boost %d %d", &onTime, &offTime) == 2) {
        // Input in seconds, but comparison in milliseconds
        boostController.boostOnTime = onTime * 1000;
        boostController.boostOffTime = offTime * 1000;
        boostController.inSwitchBoostMode = true;
    }
    else if (strncmp(input, "switch off", 10) == 0) {
        boostController.inSwitchBoostMode = false;
    }
    else {
        HALundefined(input);
    }
}

static bool isLeakDetected(double meanADC) { return (meanADC <= LEAK_THRESHOLD) ? true : false; }

static void printLeaks(int16_t *pData, int noOfChannels, int noOfSamples) {
    uint32_t status = bsGetStatus();

    if (!isUsbPortOpen()) {
        return;
    }

    if (status & BS_VERSION_ERROR_Msk) {
        USBnprintf("0x%08" PRIx32, status);
        return;
    }

    char buf[150] = {0};
    int len = 0;

    // Take mean of ADC measurements
    for (int i = 0; i <= 9; i++) {
        len += sprintf(&buf[len], "%.2f, ", ADCMean(pData, i));
    }

    // Leak detection
    for (int i = 0; i <= 9; i++) {
        len += sprintf(&buf[len], "%d, ", isLeakDetected(ADCMean(pData, i)));
    }

    // Vref ADC + board status
    //len += sprintf(&buf[len], "%d, 0x%08" PRIx32, stmGetGpio(VrefGpio), status);
    USBnprintf(buf);
}

static void toggleBoostPin() {
    // Turn on and off boost module after user defined time intervals.
    bool switchControl = (boostController.isOn) ? false : true;
    int switchTime =
        (boostController.isOn) ? boostController.boostOnTime : boostController.boostOffTime;

    if (HAL_GetTick() - boostController.timeStamp >= switchTime) {
        boostController.isOn = switchControl;
        boostController.timeStamp = HAL_GetTick();
        stmSetGpio(BoostEn, switchControl);
    }
}

TIM_HandleTypeDef *boostTimer = NULL;
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

void gpioInit() {
    stmGpioInit(&BoostEn, BOOST_EN_GPIO_Port, BOOST_EN_Pin, STM_GPIO_OUTPUT);
    //stmSetGpio(boostGpio, true);
}

void saltleakInit(ADC_HandleTypeDef *hadc1, TIM_HandleTypeDef *htim5) {
    initCAProtocol(&caProto, usbRx);

    /* Don't allow NULL handles to float about if its the wrong board type */
    boostTimer = htim5;

    /* ADC must be initialised for USB printout to work. Doesn't matter if the
     *board type is wrong,
     ** the readings will just be bogus */
    ADCMonitorInit(hadc1, ADCBuffer, sizeof(ADCBuffer) / sizeof(int16_t));

    if (-1 == boardSetup(SaltLeak, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR}, 0)) {
        return;
    }

    gpioInit();
    HAL_TIM_Base_Start_IT(boostTimer);
}

static ADCCallBack adcCbFunc = printLeaks;
void saltleakLoop(const char *bootMsg) {
    CAhandleUserInputs(&caProto, bootMsg);
    updateBoardStatus();

    ADCMonitorLoop(adcCbFunc);
}
