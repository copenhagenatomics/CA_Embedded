/*
 * si7051.c
 *
 *  Created on: Jul 26, 2021
 *      Author: alema
 */

#include "si7051.h"

#define SI7051_I2C_ADDR          0x40
#define SI7051_TEMPERATUR_OFFSET 0xF3

HAL_StatusTypeDef si7051Temp(I2C_HandleTypeDef *hi2c1, float* siValue)
{
    uint8_t readSensorADDR = SI7051_TEMPERATUR_OFFSET;

    // Poll I2C device
    HAL_StatusTypeDef  ret = HAL_I2C_Master_Transmit(hi2c1, (uint16_t)(SI7051_I2C_ADDR << 1), &readSensorADDR, 1, 50);
    if (HAL_OK == ret)
    {
        HAL_Delay(10); //delay is needed for response time

        uint8_t addata[2];
        ret = HAL_I2C_Master_Receive(hi2c1, (SI7051_I2C_ADDR << 1) | 0x01, addata, 2, 50);
        if (HAL_OK == ret)
        {
            uint16_t si7051_temp = addata[0] << 8 | addata[1];
            *siValue = (175.72*si7051_temp) / 65536 - 46.85;
        }
    }

    return ret;
}
