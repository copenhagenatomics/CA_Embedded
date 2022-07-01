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
#include "USBprint.h"

StmGpio IRsender;

static struct {
	bool isCommandIssued;
	bool isStartCommandSent;
	bool isCommReady;
} commandState = {false, false, false};

static union IRCommand {
    struct {
        uint32_t address;
        uint32_t tempData;
        uint32_t miscStates;
        uint32_t unknown;
    }; // struct used for altering data
    uint32_t command[4]; // command used for sending
} IRCommand= {.address = IR_ADDRESS, .tempData = TEMP_18, .miscStates = FAN_HIGH, .unknown = 0x20000};

static void sendCommand()
{
	static int sendBitIdx = 0;
	static int wordIdx = 0;

	if (!commandState.isStartCommandSent)
	{
		commandState.isStartCommandSent = true;
		TIM3->ARR = START_BIT_ARR;
		TIM3->CCR2 = START_BIT_CCR;
		return;
	}

	// End of message
	if (wordIdx == 3 && sendBitIdx == 17)
	{
		commandState.isCommandIssued = false;
		commandState.isStartCommandSent = false;
		commandState.isCommReady = false;

		wordIdx = 0;
		sendBitIdx = 0;

		TIM3->ARR = START_BIT_ARR;
		TIM3->CCR2 = 0;

		stmSetGpio(IRsender, true);
		return;
	}

	// After overload of counter the new PWM settings will be set
	// that codes for a logical 1 and 0 respectively.
	if (IRCommand.command[wordIdx] & (1 << (31-sendBitIdx)))
	{
		TIM3->ARR = HIGH_BIT_ARR;
		TIM3->CCR2 = HIGH_BIT_CCR;
	}
	else
	{
		TIM3->ARR = LOW_BIT_ARR;
		TIM3->CCR2 = LOW_BIT_CCR;
	}
	sendBitIdx++;

	if (sendBitIdx % 32 == 0)
	{
		sendBitIdx = 0;
		wordIdx += 1;
	}
}

void pwmGPIO()
{
	if (!commandState.isCommReady)
		return;

	if (TIM3->CNT < TIM3->CCR2 && TIM2->CNT < TIM2->CCR1)
	{
		stmSetGpio(IRsender, false);
	}
	else
	{
		stmSetGpio(IRsender, true);
	}
}

void updateTemperatureIR(int temp)
{
	//IRCommand.tempData = temp;
	commandState.isCommandIssued = true;
}

void updateFanIR(int fanSpeed)
{
	//IRCommand.miscStates = fanSpeed;
	commandState.isCommandIssued = true;
}

void turnOnLED()
{
	stmSetGpio(IRsender, true);
}

void turnOffLED()
{
	stmSetGpio(IRsender, false);
}
// Callback: timer has rolled over
static TIM_HandleTypeDef* timSignal = NULL;
static TIM_HandleTypeDef* timFreqCarrier = NULL;
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if (!commandState.isCommandIssued)
		return;

	if (commandState.isStartCommandSent && !commandState.isCommReady)
	{
		commandState.isCommReady = true;
		return;
	}

    if (htim == timSignal)
    {
        sendCommand();
    }
}

void initGPIO()
{
    stmGpioInit(&IRsender, IRsender_GPIO_Port, IRsender_Pin, STM_GPIO_OUTPUT);
    stmSetGpio(IRsender, true);
}

void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_)
{
	HAL_TIM_PWM_Start_IT(timFreqCarrier, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start_IT(timSignal_, TIM_CHANNEL_2);
	timSignal = timSignal_;
	timFreqCarrier = timFreqCarrier_;

	initGPIO();
}

