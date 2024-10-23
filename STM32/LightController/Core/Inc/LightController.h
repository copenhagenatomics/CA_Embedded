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

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define LED_CHANNELS 3
#define NO_COLORS 4
#define MAX_PWM 255

#define ACTUATION_TIMEOUT   30000 // The output times out after 30 seconds if no new input is received

/* LightController board status register definitions */
#define LIGHT_PORT_STATUS_Msk(x)    (1U << (x))

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void LightControllerInit(TIM_HandleTypeDef *htim2, TIM_HandleTypeDef *htim5, WWDG_HandleTypeDef *hwwdg);
void LightControllerLoop(const char* bootMsg);

#endif /* INC_LIGHTCONTROLLER_H_ */
