/*
 * LightController.c
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#include "LightController.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "systemInfo.h"
#include "USBprint.h"
#include "usb_cdc_fops.h"
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>


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

bool isInputValid(const char *input, int *channel, unsigned int *rgb)
{
	if (sscanf(input, "p%d %x", channel, rgb) != 2)
	{
		return false;
	}

	if (*channel <= 0 || *channel > LED_CHANNELS)
		return false;

	// Check the RGB format is exactly 6 hex characters long
    char* idx = index(input, ' ');
	if (strlen(&idx[1]) != 6)
		return false;

	// Check input is valid hex format
	for (int i = 0; i<6; i++)
	{
		if(!isxdigit((unsigned char)input[3+i]))
		{
			return false;
		}
	}

	uint8_t red = (*rgb >> 16) & 0xFF;
	if (red < 0 || red > MAX_PWM)
		return false;

	uint8_t green = (*rgb >> 8) & 0xFF;
	if (green < 0 || green > MAX_PWM)
		return false;

	uint8_t blue = *rgb & 0xFF;
	if (blue < 0 || blue > MAX_PWM)
		return false;

	return true;
}

int handleInput(unsigned int rgb, int *channel, uint8_t *red, uint8_t *green, uint8_t *blue)
{
	*red = (rgb >> 16) & 0xFF;
	*green = (rgb >> 8) & 0xFF;
	*blue = rgb & 0xFF;
	return (rgb == 0xFFFFFF) ? 1 : 0; // If 0xFFFFFF turn on the white in other ports
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
	int channel = 1;
	unsigned int rgb = 0x000000;

	if (!isInputValid(input, &channel, &rgb))
	{
		HALundefined(input);
		return;
	}

	uint8_t red, green, blue;
	int ret = handleInput(rgb, &channel, &red, &green, &blue);

	// If the user specifies white then turn on white pin separately and turn off
	// other LED lights
	for (int i = 1; i <= LED_CHANNELS; i++)
	{
		if (i == channel)
			(ret == 0) ? updateLED(i, red, green, blue) : updateLED(i, 0, 0, 0);
		else
			(ret == 0) ? updateLED(i, 0, 0, 0) : updateLED(i, red, 0, 0);
	}

	rgbs[channel-1] = rgb;
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
static WWDG_HandleTypeDef* hwwdg_ = NULL;
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    // loopTimer corresponds to htim5 which triggers with a frequency of 10Hz
    if (htim == loopTimer)
    {
    	HAL_WWDG_Refresh(hwwdg_);
        printStates();
    }
}

// Initialize board
void LightControllerInit(TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3, TIM_HandleTypeDef *htim4, TIM_HandleTypeDef *htim5, WWDG_HandleTypeDef *hwwdg)
{
	initCAProtocol(&caProto, usb_cdc_rx);

    BoardType board;
    if (getBoardInfo(&board, NULL) || board != LightController)
    {
        return;
    }

	// Start LED PWM counters
	pwmInit(htim2);
	pwmInit(htim3);
	pwmInit(htim4);

    HAL_TIM_Base_Start_IT(htim5);

	htim2_ = htim2;
	htim3_ = htim3;
	htim4_ = htim4;
	loopTimer = htim5;

	hwwdg_ = hwwdg;

	// Initialize random number generator
	srand(time(NULL));
}

// Main loop - Board only reacts on user inputs
void LightControllerLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
}
