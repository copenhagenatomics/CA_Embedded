/*!
 *  @file humidityApp.h
 *
 *  @date Apr 6, 2022
 *  @author matias
 */

#ifndef INC_HUMIDITYAPP_H_
#define INC_HUMIDITYAPP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define SHT45_ERROR_Msk(x)    (1U << (x))
#define HUMIDITY_NO_ERROR_Msk (BS_SYSTEM_ERRORS_Msk | 0x03)

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void InitHumidity(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2, CRC_HandleTypeDef* hcrc,
                  const char* bootMsg);
void LoopHumidity(const char* bootMsg);

#endif /* INC_HUMIDITYAPP_H_ */
