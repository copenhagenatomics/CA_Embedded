/*
** flowChip.h
**
**  Created on: 2022
**      Author: Anders Gnistrup
**/

#ifndef _FLOWCHIP_H_
#define _FLOWCHIP_H_

#include "stm32f4xx_hal.h"

extern uint32_t _FlashAddrCal;   // Variable defined in ld linker script.
#define FLASH_ADDR_CAL ((uintptr_t) &_FlashAddrCal)

#define FLOWCHIP_WARNING_NO_CAL_Msk     0x00000001U
#define FLOWCHIP_ERROR_WRONG_OTP_Msk    0x00000002U

/* Define showing which bits are "errors" and which are only for information */
#define FLOWCHIP_ERROR_Msk          (BS_SYSTEM_ERRORS_Msk | FLOWCHIP_ERROR_WRONG_OTP_Msk)

HAL_StatusTypeDef flowChipInit(I2C_HandleTypeDef *ctx, WWDG_HandleTypeDef *hwwdg, CRC_HandleTypeDef *hcrc);
void flowChipLoop(const char* bootMsg);

#endif /* _FLOWCHIP_H_ */


