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

static TIM_HandleTypeDef* timFreqCarrier = NULL;
static TIM_HandleTypeDef* timSignal = NULL;

static int currentTemp = 0;

static struct {
	bool isCommandIssued;
	bool isAddressSent;
	bool isCommReady;
	uint32_t cmdTimeStamp;
} commandState = {false, false, false};

static union IRCommand {
    struct {
        uint32_t address;
        uint32_t tempData;
        uint32_t miscStates;
        uint32_t checksum;
    }; // struct used for altering data
    uint32_t command[4]; // command used for sending
} IRCommand= {.address = IR_ADDRESS, .tempData = TEMP_18, .miscStates = FAN_HIGH, .checksum = CRC18};

static void sendCommand()
{
	static int sendBitIdx = 0;
	static int wordIdx = 0;
	static int noRepeats = 0;

	if (!commandState.isAddressSent)
	{
		commandState.isAddressSent = true;
		TIM3->ARR = START_BIT_ARR;
		TIM3->CCR2 = START_BIT_CCR;
		return;
	}

	// End of message
	if (wordIdx == 3 && sendBitIdx == 17)
	{
		turnOffLED();

		// Repeat command 'NO_COMMAND_REPEATS' times to minimize risk of command
		// not being received correctly
		noRepeats = (noRepeats + 1 <= NO_COMMAND_REPEATS) ? noRepeats + 1 : 0;
		commandState.isCommandIssued = (noRepeats < NO_COMMAND_REPEATS) ? true : false;
		commandState.isAddressSent = false;
		commandState.isCommReady = false;
		commandState.cmdTimeStamp = HAL_GetTick();

		wordIdx = 0;
		sendBitIdx = 0;

		TIM3->ARR = START_BIT_ARR;
		TIM3->CCR2 = 0;
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
		wordIdx++;
	}
}

void pwmGPIO()
{
	if (!commandState.isCommReady)
		return;

	if (TIM3->CNT < TIM3->CCR2)
	{
		turnOnLED();
	}
	else
	{
		turnOffLED();
	}
}

void getACStates(int * tempState)
{
	*tempState = currentTemp;
}

static uint32_t tempCodes[8] = {TEMP_18, TEMP_19, TEMP_20, TEMP_21, TEMP_22, TEMP_23, TEMP_24, TEMP_25};
static uint32_t crcCodes[8] = {CRC18, CRC19, CRC20, CRC21, CRC22, CRC23, CRC24, CRC25};
void updateTemperatureIR(int temp)
{
	currentTemp = temp;
	if (temp == 5)
	{
		IRCommand.tempData = TEMP_5;
		IRCommand.checksum = CRC5;
		IRCommand.miscStates = FAN_HIGH_5;
	}
	else if (temp == 30)
	{
		IRCommand.tempData = TEMP_30;
		IRCommand.checksum = CRC30;
		IRCommand.miscStates = FAN_HIGH;
	}
	else
	{
		IRCommand.tempData = tempCodes[temp-18];
		IRCommand.checksum = crcCodes[temp-18];
		IRCommand.miscStates = FAN_HIGH;
	}
	commandState.isCommandIssued = true;
	commandState.cmdTimeStamp = HAL_GetTick();
}

void turnOffAC()
{
	currentTemp = 0;

	IRCommand.tempData = AC_OFF;
	IRCommand.miscStates = FAN_HIGH;
	IRCommand.checksum = CRC_OFF;
	commandState.isCommandIssued = true;
}

void turnOnLED()
{
	HAL_TIM_PWM_Start(timFreqCarrier, TIM_CHANNEL_1);
}

void turnOffLED()
{
	HAL_TIM_PWM_Stop(timFreqCarrier, TIM_CHANNEL_1);
}

// Callback: timer has rolled over
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if (!commandState.isCommandIssued || (HAL_GetTick() - commandState.cmdTimeStamp) < 100)
		return;

	if (commandState.isAddressSent && !commandState.isCommReady)
	{
		commandState.isCommReady = true;
		return;
	}

    if (htim == timSignal)
    {
        sendCommand();
    }
}


void initTransmitterIR(TIM_HandleTypeDef *timFreqCarrier_, TIM_HandleTypeDef *timSignal_)
{
	HAL_TIM_PWM_Start_IT(timSignal_, TIM_CHANNEL_2);
	timSignal = timSignal_;
	timFreqCarrier = timFreqCarrier_;
}

