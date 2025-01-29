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
#define NUM_SENSORS	2

#define HEATING_INTERVAL           60000 // 1 minute
#define HEATING_SETTLING_TIME      55000 // 55 seconds
#define RH_THRESHOLD_HIGH             85 // Start heating at 85% RH

#define MAX_TEMP_BEFORE_HEATING       80 // Maximum temperature before heating
#define BURN_IN_TIME             4800000 // 80 minutes

#define SHT45_ERROR_Msk(x)		(1U << (x))
#define HUMIDITY_NO_ERROR_Msk   (BS_SYSTEM_ERRORS_Msk | 0x03)


typedef struct Measurement{
    float temp;		// temperature
    float rh;		// relative humidity
    float ah;		// absolute humidity
} Measurement;

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void InitHumidity(I2C_HandleTypeDef* hi2c1, I2C_HandleTypeDef* hi2c2, WWDG_HandleTypeDef* hwwdg);
void LoopHumidity(const char* bootMsg);

#endif /* INC_HUMIDITYAPP_H_ */
