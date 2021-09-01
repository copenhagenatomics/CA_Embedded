/*
 * honeywellZephyrI2C.h
 *
 *  Created on: Mar 22, 2021
 *      Author: alema
 */

/*
Library:			STM32F411CE I2C honeywell zephyr
Written by:			Alexander Mizrahi-Werner (alexanderMizrahi@copenhagenAtomics.com)
Last modified:		22-03-2021
Description:
					Provides an interface to communicate with the honeywell zephyr air flow
					sensor.	Tested with both STM32F103 and STM32F411CE



*/

#ifndef INC_HONEYWELLZEPHYRI2C_H_
#define INC_HONEYWELLZEPHYRI2C_H_



#endif /* INC_HONEYWELLZEPHYRI2C_H_ */



#include "stm32f4xx_hal.h"

//initialize sensor and get sensor serial number
HAL_StatusTypeDef flowSensorInit(uint32_t *serialNB);

//read sensor data
HAL_StatusTypeDef ZephyrRead(float *flowData, uint8_t SLPM);

void flowLED(float flowData);
