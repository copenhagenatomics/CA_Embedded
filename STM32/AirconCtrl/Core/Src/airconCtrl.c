/*
 * airconCtrl.c
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "airconCtrl.h"
#include "time32.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "systemInfo.h"
//#include "stm32f4xx_hal_tim.h"
#include "transmitterIR.h"
#include "pcbversion.h"

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void handleUserCommands(const char * input);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CAProtocolCtx caProto = {
        .undefined = handleUserCommands,
        .printHeader = CAPrintHeader,
        .printStatus = NULL, /* There is no Aircon specific status info */
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

int arrIdx = 0;
uint8_t bitIndex;
static int irqCnt = 0;
static uint8_t tempCode;
static uint8_t tempCodeArr[24];

TIM_HandleTypeDef *timerCtx  = NULL; 
TIM_HandleTypeDef *loopTimer = NULL;
WWDG_HandleTypeDef *hwwdg_   = NULL;

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

static void handleUserCommands(const char * input)
{
    int temp;
    if (sscanf(input, "temp %d", &temp) == 1)
    {
        if (!updateTemperatureIR(temp))
        {
            HALundefined(input);
            return;
        };
    }
    else if (strncmp(input, "off", 3)==0) 
    {
        turnOffAC();
    }
    else {
        HALundefined(input);
    }
}

/*!
** @brief Prints the latest temperature to the USB port.
**
** If the port is not open, does nothing. If the software is on the wrong board, prints the error 
** code only.
*/
static void printACTemperature()
{
    static int temp;

    if (!isUsbPortOpen()) {
        return;
    }

    if (bsGetStatus() & BS_VERSION_ERROR_Msk) {
        USBnprintf("0x%08" PRIx32, bsGetStatus());
        return;
    }

    getACStates(&temp);
    USBnprintf("%d, 0x%08" PRIx32, temp, bsGetStatus());
}

static void finishWord(bool endOfMessage)
{
    tempCodeArr[arrIdx++] = tempCode;
    tempCode = 0;
    bitIndex = 0;

    if (endOfMessage)
    {
        for(int i = 0; i < 24; i++) {
            char buf[10] = "";
            int len = snprintf(buf, 10, "%x ", tempCodeArr[i]);
            writeUSB(buf, len);
        }
        USBnprintf("");
        arrIdx = 0;
        irqCnt = 0;
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
** @brief Decodes IR communication pulses into a set of 4 32-bit words
**
** Expected to be called every falling edge of a GPIO pin. Communication format is a start bit 
** followed by some number of 1s and 0s. 
**   * Start bit = Long high followed by long low (_|----|____)
**   * 1         = Short high followed by long low (_|-|___)
**   * 0         = Short high followed by short low (_|-|_)
**
** Requirements: htim1 expected to be running at 100 kHz (e.g. each count is 10us)
*/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    static const int LEN_START_BIT = 400;
    static const int LEN_HIGH_BIT  = 160;
    static const int LEN_LOW_BIT   = 80;
    static const int BITS_PER_BYTE = 8;

    if (GPIO_Pin == GPIO_PIN_7)
    {
        irqCnt++;

        /* The high and low periods can be bunched together and read on the falling edge */
        if (__HAL_TIM_GET_COUNTER(timerCtx) > LEN_START_BIT)
        {
            tempCode = 0;
            bitIndex = 0;
        }
        else if (__HAL_TIM_GET_COUNTER(timerCtx) > LEN_HIGH_BIT)
        {
            tempCode |= (1UL << ((BITS_PER_BYTE - 1) - bitIndex));   // write 1
            bitIndex++;
        }
        else if (__HAL_TIM_GET_COUNTER(timerCtx) > LEN_LOW_BIT)
        {
            tempCode &= ~(1UL << ((BITS_PER_BYTE - 1) - bitIndex));  // write 0
            bitIndex++;
        }

        if ((bitIndex == BITS_PER_BYTE))
        {
            finishWord(false);
        }

        __HAL_TIM_SET_COUNTER(timerCtx, 0);
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == loopTimer)
    {
        HAL_WWDG_Refresh(hwwdg_);
        printACTemperature();
    }
}

void airconCtrlInit(TIM_HandleTypeDef *ctx, TIM_HandleTypeDef *loopTimer_, WWDG_HandleTypeDef *hwwdg)
{
    initCAProtocol(&caProto, usbRx);

    // Pin out has changed from PCB V1.8 - older versions need other software.
    if(-1 == boardSetup(AirCondition, (pcbVersion){BREAKING_MAJOR, BREAKING_MINOR})) {
        return;
    }

    hwwdg_    = hwwdg;
    loopTimer = loopTimer_;
    timerCtx  = ctx;

    HAL_TIM_Base_Start(timerCtx);
    __HAL_TIM_SET_COUNTER(timerCtx, 0);
}

void airconCtrlLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    pwmGPIO();

    /* Only prints if there is some message unprinted after 90 ms of inactivity */
    if ((__HAL_TIM_GET_COUNTER(timerCtx) > 9000) && (arrIdx != 0)) {
        finishWord(true);
    }
}
