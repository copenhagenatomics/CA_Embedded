/*
 * LightController.h
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#ifndef INC_LIGHTCONTROLLER_H_
#define INC_LIGHTCONTROLLER_H_

#include <stdbool.h>
#include "main.h"

#define LED_CHANNELS 3
#define MAX_PWM 256

#define ACTUATION_TIMEOUT 30000 // The output times out after 30 seconds if no new input is received

bool isInputValid(const char *input, int *channel, unsigned int *rgb);
int handleInput(unsigned int rgb, int *channel, uint8_t *red, uint8_t *green, uint8_t *blue);

void LightControllerInit(TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim3, TIM_HandleTypeDef *htim4, TIM_HandleTypeDef *htim5, WWDG_HandleTypeDef *hwwdg);
void LightControllerLoop(const char* bootMsg);

#endif /* INC_LIGHTCONTROLLER_H_ */
