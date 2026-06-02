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

#define DC_BOARD_NUM_PORTS 6

/* Each port can be on or off */
#define DC_BOARD_PORT_x_STATUS_Msk(x) (1U << (x))
#define DC_BOARD_PORTS_STATUS_Msk     ((1U << DC_BOARD_NUM_PORTS) - 1U)

/* Per-channel e-fuse overcurrent bits (1-indexed port x occupies bit 6+x, i.e. bits 7–12) */
#define DC_EFUSE_OVERCURRENT_Msk(x)  (1UL << (DC_BOARD_NUM_PORTS + (uint32_t)(x)))
#define DC_EFUSE_OVERCURRENT_ALL_Msk (0x3FUL << 7UL)

/* Define showing which bits are "errors" and which are only for information */
#define DC_BOARD_No_Error_Msk (BS_SYSTEM_ERRORS_Msk | DC_EFUSE_OVERCURRENT_ALL_Msk)

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void DCBoardInit(ADC_HandleTypeDef* _hadc, WWDG_HandleTypeDef* _hwwdg);
void DCBoardLoop(const char* bootMsg);

#endif /* SRC_DCBOARD_H_ */
