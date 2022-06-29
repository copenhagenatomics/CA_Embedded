/*
 * transmitterIR.c
 *
 *  Created on: Jun 28, 2022
 *      Author: matias
 */

#include "transmitterIR.h"
#include <stdbool.h>
#include "StmGpio.h"
#include "main.h"

StmGpio IRsender;

static struct {
	bool isCommandIssued;
	bool isStartCommandSent;
} commandState = {false, false};

static union IRCommand {
    struct {
        uint32_t address;
        uint32_t tempData;
        uint32_t miscStates;
        uint32_t unknown;
    }; // struct used for altering data
    uint32_t command[4]; // command used for sending
} IRCommand= {.address = IR_ADDRESS, .tempData = TEMP_18, .miscStates = FAN_MID, .unknown = 0};

static void sendCommand()
{
	static int sendBitIdx = 0;

	if (!commandState.isStartCommandSent)
	{
		commandState.isStartCommandSent = true;

		TIM3->PSC = START_BIT_PSC;
		TIM3->ARR = START_BIT_CCR;
		TIM3->CCR1 = START_BIT_ARR;

		return;
	}

	if (sendBitIdx > sizeof(IRCommand.command)*8)
	{
		sendBitIdx = 0;
		commandState.isCommandIssued = false;
		commandState.isStartCommandSent = false;
		return;
	}

	// After overload of counter the new PWM settings will be set
	// that codes for a logical 1 and 0 respectively.
	TIM3->PSC = OTHER_BIT_PSC;
	if (*IRCommand.command & (1 << sendBitIdx))
	{
		TIM3->ARR = HIGH_BIT_ARR;
		TIM3->CCR1 = HIGH_BIT_CCR;
	}
	else
	{
		TIM3->ARR = LOW_BIT_ARR;
		TIM3->CCR1 = LOW_BIT_CCR;
	}
	sendBitIdx++;
}

static void pwmGPIO()
{
	if (TIM3->CNT < TIM3->CCR1 && TIM1->CNT < TIM1->CCR1)
	{
		stmSetGpio(IRsender, true);
	}
	else
	{
		stmSetGpio(IRsender, false);
	}
}

void updateTemperatureIR(int temp)
{
	IRCommand.tempData = temp;
	commandState.isCommandIssued = true;
}

void updateFanIR(int fanSpeed)
{
	IRCommand.miscStates = fanSpeed;
	commandState.isCommandIssued = true;
}

// Callback: timer has rolled over
static TIM_HandleTypeDef* timSignal = NULL;
static TIM_HandleTypeDef* timFreqCarrier = NULL;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim == timFreqCarrier)
		pwmGPIO();

    if (commandState.isCommandIssued && htim == timSignal)
        sendCommand();
}

void initGPIO()
{
    stmGpioInit(&IRsender, IRsender_GPIO_Port, IRsender_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(IRsender, false);
}

void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_)
{
	HAL_TIM_PWM_Start(timFreqCarrier, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(timSignal_, TIM_CHANNEL_1);
	timSignal = timSignal_;
	timFreqCarrier = timFreqCarrier_;

	initGPIO();
}
