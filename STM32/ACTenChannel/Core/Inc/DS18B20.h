/*
 * DS18B20.h
 *
 *  Created on: 7 Nov 2024
 *      Author: matias
 */

#ifndef SRC_DC18B20_H_
#define SRC_DC18B20_H_

#include "stm32f4xx_hal.h"
#include "StmGpio.h"

/***************************************************************************************************
** PUBLIC FUNCTION PROTOTYPES
***************************************************************************************************/

float getTemp();
void DS18B20Init(TIM_HandleTypeDef* htim, StmGpio* gpio, GPIO_TypeDef *blk, uint16_t pin);

#endif /* SRC_DC18B20_H_ */