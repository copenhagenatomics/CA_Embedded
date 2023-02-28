/*
 * DAC7811.h
 *
 *  Created on: Feb 23, 2023
 *      Author: matias
 */

#ifndef INC_DAC7811_H_
#define INC_DAC7811_H_

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include <StmGpio.h>

typedef struct DAC7811Device {
    SPI_HandleTypeDef* hspi; // Pointer to SPI interface.
    StmGpio cs;              // Chip select for the specific device.
    uint16_t outputValue;
} DAC7811Device;

/* Description: Configure a single DAC7811 device. Configuration is fixed
 * @param dev pointer to DAC7811 device.
 * @return     0 on success, else negative value (See code for values)
 */
int DAC7811Init(DAC7811Device *dev);

HAL_StatusTypeDef readOutputValue(DAC7811Device *dev);
HAL_StatusTypeDef loadAndUpdate(DAC7811Device *dev, uint16_t data);
HAL_StatusTypeDef resetMidScale(DAC7811Device *dev);
HAL_StatusTypeDef readOutputValue(DAC7811Device *dev);

#endif /* INC_DAC7811_H_ */
