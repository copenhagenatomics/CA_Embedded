/*
 * saltleak.h
 *
 *  Created on: Nov 11, 2021
 *      Author: agp
 */

#ifndef SALT_LEAK_LOOP_H_
#define SALT_LEAK_LOOP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define BOOST_ACTIVE_Msk 0x00000001U
#define BOOST_PIN_HIGH_Msk 0x00000002U

#define SALTLEAK_NO_ERROR_Msk (BS_SYSTEM_ERRORS_Msk)

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void saltleakInit(ADC_HandleTypeDef* hadc1, TIM_HandleTypeDef* htim5);
void saltleakLoop(const char* bootMsg);

#endif /* SALT_LEAK_LOOP_H_ */