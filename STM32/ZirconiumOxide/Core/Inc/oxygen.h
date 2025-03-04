/*
 * oxygen.h
 *
 *  Created on: Apr 12, 2022
 *      Author: matias
 */

#ifndef INC_OXYGEN_H_
#define INC_OXYGEN_H_

#include <stdbool.h>

/***************************************************************************************************
** DEFINES
***************************************************************************************************/
#define HIGH_RANGE_SENSOR_ON_Pos        0U
#define HIGH_RANGE_SENSOR_ON_Msk        (1U << HIGH_RANGE_SENSOR_ON_Pos)

#define LOW_RANGE_SENSOR_ON_Pos         1U
#define LOW_RANGE_SENSOR_ON_Msk         (1U << LOW_RANGE_SENSOR_ON_Pos)

#define BELOW_RANGE_STATUS_Pos          2U
#define BELOW_RANGE_STATUS_Msk          (1U << BELOW_RANGE_STATUS_Pos)

#define HIGH_RANGE_HEATER_ERROR_Pos     3U
#define HIGH_RANGE_HEATER_ERROR_Msk     (1U << HIGH_RANGE_HEATER_ERROR_Pos)

#define LOW_RANGE_HEATER_ERROR_Pos      4U
#define LOW_RANGE_HEATER_ERROR_Msk      (1U << LOW_RANGE_HEATER_ERROR_Pos)

#define ZRO2_No_Error_Msk               (BS_SYSTEM_ERRORS_Msk | LOW_RANGE_HEATER_ERROR_Msk | HIGH_RANGE_HEATER_ERROR_Msk)

#define HIGH_HEATER_TRANSITION_LEVEL    800
#define LOW_HEATER_TRANSITION_LEVEL     20000

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void oxygenInit(ADC_HandleTypeDef* hadc_, TIM_HandleTypeDef* adcTimer, TIM_HandleTypeDef* loopTimer_, CRC_HandleTypeDef* hcrc);
void oxygenLoop(const char* bootMsg);

#endif /* INC_OXYGEN_H_ */
