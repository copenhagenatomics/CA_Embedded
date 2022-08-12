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


TIM_HandleTypeDef * htim2_ = NULL;
TIM_HandleTypeDef * htim3_ = NULL;
TIM_HandleTypeDef * htim4_ = NULL;
static void updateLED(int channel, unsigned int red, unsigned int green, unsigned int blue, unsigned int white)
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
	unsigned int red;
	unsigned int green;
	unsigned int blue;
	unsigned int white;

	if (sscanf(input, "p%d %d %d %d %d", &channel, &red, &green, &blue, &white) == 4)
	{
		updateLED(channel, red, green, blue, white);
	}
	else
	{
		HALundefined(input);
	}
}

// Initialise PWM group
static void pwmInit(TIM_HandleTypeDef * htim)
{
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start_IT(htim, TIM_CHANNEL_3);
}

// Initialize board
void LightControllerInit(TIM_HandleTypeDef * htim2, TIM_HandleTypeDef * htim3, TIM_HandleTypeDef * htim4)
{
	initCAProtocol(&caProto, usb_cdc_rx);
	// Start LED PWM counters
	pwmInit(htim2);
	pwmInit(htim3);
	pwmInit(htim4);

	htim2_ = htim2;
	htim3_ = htim3;
	htim4_ = htim4;
}

// Main loop - Board only reacts on user inputs
void LightControllerLoop(const char* bootMsg)
{
    CAhandleUserInputs(&caProto, bootMsg);
}
