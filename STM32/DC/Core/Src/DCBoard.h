/*
 * DCBoard.h
 *
 *  Created on: 6 Jan 2022
 *      Author: agp
 */

#ifndef SRC_DCBOARD_H_
#define SRC_DCBOARD_H_

#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* DC board status register definitions */

/* Each of the 5 ports (1x Fan + 4x AC) can be on or off */
#define DC_BOARD_PORT_x_STATUS_Msk(x)  (1U << (x))
#define DC_BOARD_PORTS_STATUS_Msk     ((1U << 5U) - 1U)

/* Define showing which bits are "errors" and which are only for information */
#define DC_BOARD_No_Error_Msk         (BS_SYSTEM_ERRORS_Msk)

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void DCBoardInit(ADC_HandleTypeDef *_hadc, WWDG_HandleTypeDef* _hwwdg);
void DCBoardLoop(const char* bootMsg);

#endif /* SRC_DCBOARD_H_ */
