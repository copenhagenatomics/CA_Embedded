/*
 * LightController.c
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#include "LightController.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "USBprint.h"
#include "usb_cdc_fops.h"
#include <stdbool.h>


static unsigned int rgbs[LED_CHANNELS] = {0, 0, 0};

static void controlLEDStrip(const char *input);

static CAProtocolCtx caProto =
{
        .undefined = controlLEDStrip,
        .printHeader = CAPrintHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead = CAotpRead,
        .otpWrite = NULL
};

static bool isInputValid(int channel, unsigned int red, unsigned int green, unsigned int blue)
{
	if (channel <= 0 || channel > LED_CHANNELS)
		return false;

	if (red < 0 || red > MAX_PWM)
		return false;

	if (green < 0 || green > MAX_PWM)
		return false;

	if (blue < 0 || blue > MAX_PWM)
		return false;

	return true;
}

TIM_HandleTypeDef * htim2_ = NULL;
TIM_HandleTypeDef * htim3_ = NULL;
TIM_HandleTypeDef * htim4_ = NULL;
static void updateLED(int channel, unsigned int red, unsigned int green, unsigned int blue)
{
	TIM_HandleTypeDef * portHandle = NULL;
	switch (channel)
	{
		case 1:
		portHandle = htim2_;
		break;

		case 2:
		portHandle = htim3_;
		break;

		case 3:
		portHandle = htim4_;
		break;
	}

	portHandle->Instance->CCR1 = red;
	portHandle->Instance->CCR2 = green;
	portHandle->Instance->CCR3 = blue;
}

// Update LED strip with user input colors
static void controlLEDStrip(const char *input)
{
	int channel;
	unsigned int rgb;

	if (sscanf(input, "p%d %x", &channel, &rgb) == 2)
	{
		uint8_t red = (rgb >> 16) & 0xFF;
		uint8_t green = (rgb >> 8) & 0xFF;
		uint8_t blue = rgb & 0xFF;

		if (!isInputValid(channel, red, green, blue))
		{
			HALundefined(input);
			return;
		}

		updateLED(channel, red, green, blue);
		rgbs[channel-1] = rgb;
	}
	else
	{
		HALundefined(input);
	}
}

static void printStates()
{
	if (!isComPortOpen())
		return;

	USBnprintf("%x, %x, %x", rgbs[0], rgbs[1], rgbs[2]);
}

// Initialise PWM group
static void pwmInit(TIM_HandleTypeDef * htim)
{
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_3);
}

// Callback: timer has rolled over
static TIM_HandleTypeDef* loopTimer = NULL;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // loopTimer corresponds to htim5 which triggers with a frequency of 10Hz
    if (htim == loopTimer)
    {
        printStates();
    }
}

// Initialize board
void LightControllerInit(TIM_HandleTypeDef * htim2, TIM_HandleTypeDef * htim3, TIM_HandleTypeDef * htim4, TIM_HandleTypeDef * htim5)
{
	initCAProtocol(&caProto, usb_cdc_rx);
	// Start LED PWM counters
	pwmInit(htim2);
	pwmInit(htim3);
	pwmInit(htim4);

    HAL_TIM_Base_Start_IT(htim5);

	htim2_ = htim2;
	htim3_ = htim3;
	htim4_ = htim4;
	loopTimer = htim5;
}

// Main loop - Board only reacts on user inputs
void LightControllerLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
}
