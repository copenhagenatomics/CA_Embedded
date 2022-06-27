/*
 * MCP4725.c
 *
 *  Created on: Jun 22, 2022
 *      Author: matias
 */

#include "MCP4725.h"
#include <assert.h>
#include <stdbool.h>


static uint16_t uint8Merge(uint8_t msb, uint8_t lsb){
	return (uint16_t)((uint16_t) msb << 8u) | lsb;
}

bool MCP4725_init(MCP4725_handle *handle, I2C_HandleTypeDef * hi2c)
{
	bool i2cErr = false;
	handle->i2c_handle = hi2c;
	HAL_StatusTypeDef ret = MCP4725_setValue(handle, 0);

	if (ret != HAL_OK)
	{
    	i2cErr = true;
	}
	return i2cErr;
}

HAL_StatusTypeDef MCP4725_setValue(MCP4725_handle *handle, uint16_t value)
{
	if (handle->i2c_handle == NULL)
		return HAL_ERROR;

	if (value > MCP4725_RES || value < 0)
		return HAL_ERROR;

	uint8_t command[2] = {(value & 0xff00) >> 8u, value & 0x00ff};
	HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(handle->i2c_handle, handle->device_address << 1u, command, sizeof(command), 10);

	return ret;
}


HAL_StatusTypeDef MCP4725_getValue(MCP4725_handle *handle, uint16_t *value)
{
	if (handle->i2c_handle == NULL)
		return HAL_ERROR;

	uint8_t buffer[5];

	HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(handle->i2c_handle, (handle->device_address  << 1u) | 0x01, buffer, sizeof(buffer), 10);
	if (ret != HAL_OK)
		return ret;

	*value = (uint8Merge(buffer[1], buffer[2]) >> 4);
	return HAL_OK;
}

