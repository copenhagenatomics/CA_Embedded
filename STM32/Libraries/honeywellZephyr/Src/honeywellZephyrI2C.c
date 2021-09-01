
/*
Library:			STM32F411CE I2C honeywell zephyr
Written by:			Alexander Mizrahi-Werner (alexanderMizrahi@copenhagenAtomics.com)
Last modified:		22-03-2021
Description:
					Provides an interface to communicate with the honeywell zephyr air flow
					sensor.	Tested with both STM32F103 and STM32F411CE



*/
#include "honeywellZephyrI2C.h"
#include "stm32f4xx_hal.h"
#include "string.h"

const uint8_t zephyrADDR = 0x49;
const uint8_t readSerialNBADDR = 0x01;
const uint8_t readSensorADDR = 0x00;

I2C_HandleTypeDef hi2c1;

HAL_StatusTypeDef ZephyrRead(float *flowData, uint8_t SLPM){
	uint8_t addata[2];
	int32_t placeHolder;
	HAL_StatusTypeDef ret;
	ret = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(zephyrADDR << 1), &readSensorADDR, 2, 50);
	if(ret != HAL_OK){
		return ret;
	}

	ret = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(zephyrADDR << 1)|0x01, addata, 2, 50);
	if(ret != HAL_OK){
		return ret;
	}

	placeHolder = ((uint16_t)addata[0]<< 8) | addata[1];

    *flowData = SLPM * ((placeHolder/16384.0) - 0.1)/0.8;

	return HAL_OK;
}

HAL_StatusTypeDef flowSensorInit(uint32_t *serialNB){
/*
 * The sensor prints out 2 bytes on startup with its serial number
 *
 */
	uint8_t addata1[4];
	HAL_StatusTypeDef ret;
	ret = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(zephyrADDR << 1), &readSerialNBADDR, 1, 50);
	if(ret != HAL_OK){
		return ret;
	}
	HAL_Delay(2);
	ret = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(zephyrADDR << 1)|0x01, &addata1[0], 2, 50);
	if(ret != HAL_OK){
		return ret;
	}

	HAL_Delay(10);
	ret = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(zephyrADDR << 1)|0x01, &addata1[2], 2, 50);
	if(ret != HAL_OK){
		return ret;
	}

	*serialNB = ((addata1[0] & 0xFFFFFFFF) << 24) | ((addata1[1] & 0xFFFFFFFF) << 16) | ((addata1[2] & 0xFFFFFFFF) << 8) | addata1[3];
	return ret;
}
