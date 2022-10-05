/*
 * airconCtrl.c
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */
#include "airconCtrl.h"
#include "time32.h"
#include "USBprint.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "systemInfo.h"
#include "usb_cdc_fops.h"
#include "stm32f4xx_hal_tim.h"
#include "transmitterIR.h"

static void handleUserCommands(const char * input);
static CAProtocolCtx caProto = {
        .undefined = handleUserCommands,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL, // TODO: change method for calibration?
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

// ---- ENCODING SIGNAL -----
static void handleUserCommands(const char * input)
{
	int temp;
	if (sscanf(input, "temp %d", &temp) == 1)
	{
		if ((temp < 18 || temp > 25) && temp != 5 && temp != 30)
		{
			HALundefined(input);
			return;
		}
		updateTemperatureIR(temp);
	}
	else if (strncmp(input, "off", 3)==0)
	{
		turnOffAC();
	}
	else
		HALundefined(input);
}

static void printACTemperature()
{
	static int temp;

	if (!isComPortOpen()) return;

	getACStates(&temp);
	USBnprintf("%d", temp);
}


// ---- DECODING SIGNAL -----
uint32_t tempCode;
uint32_t tempCodeArr[4];
int arrIdx = 0;
uint8_t bitIndex;
TIM_HandleTypeDef *timerCtx;

static int irqCnt;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7)
    {
        irqCnt++;
        if (__HAL_TIM_GET_COUNTER(timerCtx) > 400)
        {
            tempCode = 0;
            bitIndex = 0;
        }
        else if (__HAL_TIM_GET_COUNTER(timerCtx) > 160)
        {
            tempCode |= (1UL << (31 - bitIndex));   // write 1
            bitIndex++;
        }
        else if (__HAL_TIM_GET_COUNTER(timerCtx) > 80)
        {
            tempCode &= ~(1UL << (31 - bitIndex));  // write 0
            bitIndex++;
        }
        if (bitIndex == 32 || irqCnt == 114)
        {
        	tempCodeArr[arrIdx] = tempCode;
        	tempCode = 0;
            bitIndex = 0;

            if (arrIdx == 3)
            {
            	USBnprintf("%x %x %x %x", tempCodeArr[0], tempCodeArr[1], tempCodeArr[2], tempCodeArr[3]);
            	arrIdx = -1;
            	irqCnt = 0;
            }
            arrIdx++;
        }
        __HAL_TIM_SET_COUNTER(timerCtx, 0);
    }
}

TIM_HandleTypeDef *loopTimer = NULL;
WWDG_HandleTypeDef *hwwdg_ = NULL;
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
    BoardType board;
    if (getBoardInfo(&board, NULL) || board != AirCondition)
        return;

    hwwdg_ = hwwdg;
    loopTimer = loopTimer_;

	initCAProtocol(&caProto, usb_cdc_rx);

    timerCtx = ctx;
    HAL_TIM_Base_Start(timerCtx);
    __HAL_TIM_SET_COUNTER(timerCtx, 0);
}

void airconCtrlLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
    pwmGPIO();
}
