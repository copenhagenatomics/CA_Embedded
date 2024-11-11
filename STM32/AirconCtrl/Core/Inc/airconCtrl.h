/*
 * airconCtrl.h
 *
 *  Created on: Mar 24, 2022
 *      Author: agp
 */

#ifndef INC_AIRCONCTRL_H_
#define INC_AIRCONCTRL_H_

#include "stm32f4xx_hal.h"

#include "systemInfo.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

/* Define showing which bits are "errors" and which are only for information */
#define AIRCON_BOARD_No_Error_Msk         (BS_SYSTEM_ERRORS_Msk)

/***************************************************************************************************
** PUBLIC FUNCTION DECLARATIONS
***************************************************************************************************/

void airconCtrlInit(TIM_HandleTypeDef *ctx, TIM_HandleTypeDef *loopTimer_, WWDG_HandleTypeDef *hwwdg);
void airconCtrlLoop();

#endif /* INC_AIRCONCTRL_H_ */
