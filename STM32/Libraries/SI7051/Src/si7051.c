/*
 * si7051.c
 *
 *  Created on: Jul 26, 2021
 *      Author: alema
 */

#include "si7051.h"
I2C_HandleTypeDef hi2c1;
float si7051Temp(){
	uint8_t addata[2];
	uint16_t si7051_temp = 0;

	const uint8_t si7051ADDR = 0x40;
	const uint8_t readSensorADDR = 0xF3;

	HAL_StatusTypeDef ret;

	//Poll I2C device
	ret = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(si7051ADDR << 1), &readSensorADDR, 1, 50);
	//delay is needed for response time
	HAL_Delay(10);
	ret = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(si7051ADDR << 1)|0x01, addata, 2, 50);
	si7051_temp = addata[0] << 8 | addata[1];

	if(ret == HAL_OK){
		return (175.72*si7051_temp) / 65536 - 46.85;
	}
	else{
		return ret;

	}
}
