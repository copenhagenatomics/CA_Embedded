/*
** tachometer.h
**
**  Created on: Nov 10, 2023
**      Author: matias
**/

#ifndef INC_TACHOINPUT_H_
#define INC_TACHOINPUT_H_

#include <stm32f4xx_hal.h>

void tachoInputLoop(const char* bootMsg);
void tachoInputInit(TIM_HandleTypeDef* htimIC, TIM_HandleTypeDef* _printTimer);

#endif /* INC_TACHOINPUT_H_ */