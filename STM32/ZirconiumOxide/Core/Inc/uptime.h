/*
 * uptime.h
 *
 *  Created on: Apr 12, 2022
 *      Author: matias
 */

#ifndef INC_UPTIME_H_
#define INC_UPTIME_H_

#include <stdbool.h>
#include "stm32f4xx_hal.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

extern uint32_t _FlashAddrUptime;   // Variable defined in ld linker script.
#define FLASH_ADDR_UPTIME ((uintptr_t) &_FlashAddrUptime)

typedef struct Uptime {
    uint32_t board_uptime;
    uint32_t high_sensor_uptime;
    uint32_t high_heater_uptime;
    uint32_t low_sensor_uptime;
    uint32_t low_heater_uptime;
} Uptime;

/***************************************************************************************************
** PUBLIC FUNCTION PROTOTYPES
***************************************************************************************************/

Uptime getUptime();
void updateUptime();
void initUptime(CRC_HandleTypeDef* _hcrc);

#endif /* INC_UPTIME_H_ */