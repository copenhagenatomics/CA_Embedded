/*
 * ACTenCh.h
 *
 *  Created on: 7 Nov 2024
 *      Author: matias
 */

#ifndef SRC_ACTENCHANNEL_H_
#define SRC_ACTENCHANNEL_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* AC 10 Channel board status register definitions */

/* Is on when there is mains is not connected to the board */
#define AC_POWER_ERROR_Msk (1UL << 11U)

/* Each of the 11 ports (1x Fan + 10x AC) can be on or off */
#define AC_TEN_CH_PORT_x_STATUS_Msk(x) (1UL << (x))
#define AC_TEN_CH_PORTS_STATUS_Msk     ((1UL << 10U) - 1U)

/* Define showing which bits are "errors" and which are only for information */
#define AC_TEN_CH_No_Error_Msk (BS_SYSTEM_ERRORS_Msk | AC_POWER_ERROR_Msk)

/* Common definitions for AC 10 Channel */
#define AC_TEN_CH_NUM_PORTS 10

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void ACTenChannelInit(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* htim,
                      TIM_HandleTypeDef* hDS18B20tim);
void ACTenChannelLoop(const char* bootMsg);

#endif /* SRC_ACTENCHANNEL_H_ */
