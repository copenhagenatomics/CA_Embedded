/*
 * sht35.h
 *
 *  Created on: Apr 15, 2021
 *      Author: alexander.mizrahi@copenhagenAtomics.com
 */
#pragma once
#ifndef INC_SHT35_H_
#define INC_SHT35_H_



#endif /* INC_SHT35_H_ */
#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif
#include <stdbool.h>
#include "assert.h"
#include "string.h"


typedef struct {
	I2C_HandleTypeDef *i2c_handle;
	uint16_t device_address;
} sht3x_handle_t;

typedef enum
{
	SHT3X_COMMAND_MEASURE_HIGHREP_STRETCH = 0x2c06,
	SHT3X_COMMAND_CLEAR_STATUS = 0x3041,
	SHT3X_COMMAND_SOFT_RESET = 0x30A2,
	SHT3X_COMMAND_HEATER_ENABLE = 0x306d,
	SHT3X_COMMAND_HEATER_DISABLE = 0x3066,
	SHT3X_COMMAND_READ_STATUS = 0xf32d,
	SHT3X_COMMAND_FETCH_DATA = 0xe000,
	SHT3X_COMMAND_MEASURE_HIGHREP_10HZ = 0x2737,
	SHT3X_COMMAND_MEASURE_LOWREP_10HZ = 0x272a
} sht3x_command_t;

HAL_StatusTypeDef sht3x_send_command(sht3x_handle_t *handle, sht3x_command_t command);
HAL_StatusTypeDef sht3x_init(sht3x_handle_t *handle);
HAL_StatusTypeDef sht3x_read_temperature_and_humidity(sht3x_handle_t *handle, float *temperature, float *humidity);


