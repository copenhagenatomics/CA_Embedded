/*
 * MCP4725.h
 *
 *  Created on: Jun 22, 2022
 *      Author: matias
 */

#ifndef INC_MCP4725_H_
#define INC_MCP4725_H_

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include "MCP4725.h"
#include <stdbool.h>

// Constant
#define MCP4725_RES        4095

// page 22
#define MCP4725_GC_RESET        0x06
#define MCP4725_GC_WAKEUP       0x09


typedef struct {
	I2C_HandleTypeDef *i2c_handle;
	uint16_t device_address;
} MCP4725_handle;

bool MCP4725_init(MCP4725_handle *handle, I2C_HandleTypeDef * hi2c);
HAL_StatusTypeDef MCP4725_getValue(MCP4725_handle *handle, uint16_t *value);
HAL_StatusTypeDef MCP4725_setValue(MCP4725_handle *handle, uint16_t value);

#endif /* INC_MCP4725_H_ */
