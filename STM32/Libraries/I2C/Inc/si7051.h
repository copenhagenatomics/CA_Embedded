/*
 * si7051.h
 *
 *  Created on: Jul 26, 2021
 *      Author: alema
 */

#ifndef INC_SI7051_H_
#define INC_SI7051_H_

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
HAL_StatusTypeDef si7051Temp(I2C_HandleTypeDef *hi2c1, float* siValue);

#endif /* INC_SI7051_H_ */

