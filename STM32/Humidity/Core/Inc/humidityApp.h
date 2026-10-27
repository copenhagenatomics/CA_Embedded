/*!
 * @file    humidityApp.h
 * @brief   Header file of humidityApp.c
 * @date    06/04/2022
 * @author  matias
 */

#ifndef INC_HUMIDITYAPP_H_
#define INC_HUMIDITYAPP_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define BS_SHT45_ERROR_Msk(x)  (1U << (x))
#define BS_HUMIDITY_BURNIN_Msk 0x004U

#define BS_HUMIDITY_NO_ERROR_Msk \
    (BS_SYSTEM_ERRORS_Msk | BS_SHT45_ERROR_Msk(0) | BS_SHT45_ERROR_Msk(1))

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void InitHumidity(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2, CRC_HandleTypeDef* hcrc,
                  const char* bootMsg);
void LoopHumidity(const char* bootMsg);

#endif /* INC_HUMIDITYAPP_H_ */
