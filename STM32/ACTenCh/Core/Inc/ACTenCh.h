/*
 * ACTenCh.h
 *
 *  Created on: 7 Nov 2024
 *      Author: matias
 */

#ifndef SRC_ACTENCH_H_
#define SRC_ACTENCH_H_

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* AC 10 Channel board status register definitions */

/* Each of the 11 ports (1x Fan + 10x AC) can be on or off */
#define AC_TEN_CH_PORT_x_STATUS_Msk(x) (1U << (x))
#define AC_TEN_CH_PORTS_STATUS_Msk     ((1U << 11U) - 1U)

/* Define showing which bits are "errors" and which are only for information */
#define AC_TEN_CH_No_Error_Msk         (BS_SYSTEM_ERRORS_Msk)

/* Common definitions for AC 10 Channel */
#define AC_TEN_CH_NUM_PORTS            10

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void ACTenChannelInit(ADC_HandleTypeDef* hadc, TIM_HandleTypeDef* htim);
void ACTenChannelLoop(const char* bootMsg);

#endif /* SRC_ACTENCH_H_ */
