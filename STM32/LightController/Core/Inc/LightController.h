/*
 * LightController.h
 *
 *  Created on: Aug 12, 2022
 *      Author: matias
 */

#ifndef INC_LIGHTCONTROLLER_H_
#define INC_LIGHTCONTROLLER_H_

#include "main.h"

#define LED_CHANNELS 3

void LightControllerInit(TIM_HandleTypeDef * htim2, TIM_HandleTypeDef * htim3, TIM_HandleTypeDef * htim4, TIM_HandleTypeDef * htim5);
void LightControllerLoop(const char* bootMsg);

#endif /* INC_LIGHTCONTROLLER_H_ */
