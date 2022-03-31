/*
 * airconCtrl.h
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */

#ifndef INC_AIRCONCTRL_H_
#define INC_AIRCONCTRL_H_

#include "stm32f4xx_hal.h"

void airconCtrlInit(TIM_HandleTypeDef *ctx);
void airconCtrlLoop();

#endif /* INC_AIRCONCTRL_H_ */
