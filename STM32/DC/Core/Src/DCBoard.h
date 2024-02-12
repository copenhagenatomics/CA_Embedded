/*
 * DCBoard.h
 *
 *  Created on: 6 Jan 2022
 *      Author: agp
 */

#ifndef SRC_DCBOARD_H_
#define SRC_DCBOARD_H_

#include "stm32f4xx_hal.h"

void DCBoardInit(ADC_HandleTypeDef *_hadc, WWDG_HandleTypeDef* _hwwdg);
void DCBoardLoop(const char* bootMsg);

#endif /* SRC_DCBOARD_H_ */
