/*
 * ACBoard.h
 *
 *  Created on: 6 Jan 2022
 *      Author: agp
 */

#ifndef SRC_ACBOARD_H_
#define SRC_ACBOARD_H_

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* AC board status register definitions */

/* Each of the 5 ports (1x Fan + 4x AC) can be on or off */
#define AC_BOARD_PORT_x_STATUS_Msk(x) (1U << (x))
#define AC_BOARD_PORTS_STATUS_Msk     ((1U << 5U) - 1U)

/* Define showing which bits are "errors" and which are only for information */
#define AC_BOARD_No_Error_Msk         (BS_SYSTEM_ERRORS_Msk)

/* Common definitions for AC Board */
#define AC_BOARD_NUM_PORTS            4

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void ACBoardInit(ADC_HandleTypeDef* hadc, WWDG_HandleTypeDef* hwwdg);
void ACBoardLoop(const char* startMsg);

#endif /* SRC_ACBOARD_H_ */
