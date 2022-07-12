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
		updateTemperatureIR(temp);
	}
	else if (strncmp(input, "on", 2)==0)
	{
		turnOnLED();
	}
	else if (strncmp(input, "off", 3)==0)
	{
		turnOffLED();
	}
	else
		HALundefined(input);
}



// ---- DECODING SIGNAL -----
uint32_t tempCode;
uint32_t tempCodeArr[4];
int arrIdx = 0;
uint8_t bitIndex;
uint8_t cmd;
uint8_t cmdli;
TIM_HandleTypeDef *timerCtx;

static int tempArr[15] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30};
static uint32_t tempArrCodes[15] = {TEMP_16, TEMP_17, TEMP_18, TEMP_19, TEMP_20, TEMP_21, TEMP_22, TEMP_23, TEMP_24, TEMP_25, TEMP_26, TEMP_27, TEMP_28, TEMP_29, TEMP_30};
static int decodeTemp(uint32_t *pData)
{
	for (int i = 0; i < 15; i++)
	{
		if (*pData == tempArrCodes[i])
		{
			return tempArr[i];
		}
	}
	return -1;
}

static uint32_t fanStates[3] = {FAN_LOW, FAN_MID, FAN_HIGH};
char stateArr[3][4] = {"LOW", "MID", "HIGH"};
static void decodeFanState(uint32_t *pData, int * idx, bool * isBlowerOn)
{
	for (int i = 0; i < 3; i++)
	{
		if ((*pData & FAN_MASK) == (fanStates[i] & FAN_MASK))
		{
			*idx = i;
		}
	}

	if (((*pData & BLOWER_MASK) >> 1) >= 4)
		*isBlowerOn = true;
	else
		*isBlowerOn = false;
}

static void checkCommand(uint32_t *pData)
{
	if (*pData != IR_ADDRESS)
		return;

	int temp = decodeTemp((pData+1));
	USBnprintf("Temp: %d", temp);


	int idx;
	bool isBlowerOn;
	decodeFanState(pData+2, &idx, &isBlowerOn);

	USBnprintf("Fan State: %s", stateArr[idx]);
	USBnprintf("Is blower on: %d", isBlowerOn);
}

static int irqCnt;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_7)
    {
        irqCnt++;
        if (__HAL_TIM_GET_COUNTER(timerCtx) > 400)
        {
            irqCnt=1;
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
            	checkCommand(&tempCodeArr[0]);
            	arrIdx = -1;
            	irqCnt = 0;
            }
            arrIdx++;
        }
        __HAL_TIM_SET_COUNTER(timerCtx, 0);
    }
}


void airconCtrlInit(TIM_HandleTypeDef *ctx)
{
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
