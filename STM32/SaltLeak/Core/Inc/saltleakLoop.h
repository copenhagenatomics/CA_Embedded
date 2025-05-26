/*!
 * @file    saltleakLoop.h
 * @brief   Header file of saltleakLoop.c
 * @date    11/11/2021
 * @authors agp, Timothé Dodin
 */

#ifndef SALT_LEAK_LOOP_H_
#define SALT_LEAK_LOOP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Specific Board Status register definitions */

// Boost voltage out of nominal value
#define BS_BOOST_ERROR_Pos 2U
#define BS_BOOST_ERROR_Msk (1UL << BS_BOOST_ERROR_Pos)

// Boost controller active
#define BS_BOOST_SWITCH_Pos 1U
#define BS_BOOST_SWITCH_Msk (1UL << BS_BOOST_SWITCH_Pos)

// Boost converter enabled
#define BS_BOOST_PIN_Pos 0U
#define BS_BOOST_PIN_Msk (1UL << BS_BOOST_PIN_Pos)

// Used for defining which bits are errors, and which are statuses
#define SALTLEAK_ERRORS_Msk (BS_SYSTEM_ERRORS_Msk | BS_BOOST_ERROR_Msk)

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void saltleakInit(ADC_HandleTypeDef *hadc1, CRC_HandleTypeDef *hcrc);
void saltleakLoop(const char *bootMsg);

#endif /* SALT_LEAK_LOOP_H_ */