/*!
 * @file    saltleakLoop.h
 * @brief   Header file of saltleakLoop.c
 * @date    11/11/2021
 * @author  agp
 */

#ifndef SALT_LEAK_LOOP_H_
#define SALT_LEAK_LOOP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void saltleakInit(ADC_HandleTypeDef* hadc1);
void saltleakLoop(const char* bootMsg);

#endif /* SALT_LEAK_LOOP_H_ */