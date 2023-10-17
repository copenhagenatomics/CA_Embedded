/*!
** @file    fake_stm32xxxx_hal.h
** @author  Luke W
** @date    12/10/2023
**/

/* Prevent inclusion of real HALs, as well as re-inclusion of this one */
#ifndef __STM32xxxx_HAL_H
#define __STM32xxxx_HAL_H
#define __STM32F4xx_HAL_H
#define __STM32H7xx_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

void forceTick(uint32_t next_val);
void autoIncTick(uint32_t next_val, bool disable=false);
uint32_t HAL_GetTick(void);

#ifdef __cplusplus
}
#endif

#endif /* __STM32xxxx_HAL_H */