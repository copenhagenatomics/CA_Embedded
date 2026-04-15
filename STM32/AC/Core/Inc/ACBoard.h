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
#define AC_BOARD_PORT_x_STATUS_Msk(x) (1UL << (x))
#define AC_BOARD_PORTS_STATUS_Msk     ((1UL << 5UL) - 1UL)

/* Is on when there is mains is not connected to the board */
#define AC_POWER_ERROR_Msk           (1UL << 5UL)

/* Is on if user requests more than 10 seconds on time for a port until next request */
#define AC_LIMIT_ON_TIME_STATUS_Msk  (1UL << 6UL)              

/* Define showing which bits are "errors" and which are only for information */
#define AC_BOARD_No_Error_Msk         (BS_SYSTEM_ERRORS_Msk | AC_POWER_ERROR_Msk)

/* Common definitions for AC Board */
#define AC_BOARD_NUM_PORTS            4

/***************************************************************************************************
** PUBLIC OBJECTS
***************************************************************************************************/

extern uint32_t _FlashAddrFault;   // Variable defined in ld linker script.

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void ACBoardInit(ADC_HandleTypeDef* hadc);
void ACBoardLoop(const char* startMsg);

#endif /* SRC_ACBOARD_H_ */
