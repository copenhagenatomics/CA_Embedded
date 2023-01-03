/*
 * jumpToBootloader.h
 *
 *  Created on: Mar 22, 2021
 *      Author: alema
 */

#ifndef INC_JUMPTOBOOTLOADER_H_
#define INC_JUMPTOBOOTLOADER_H_

#if defined(STM32F401xC)
#include "stm32f4xx_hal.h"
#elif defined(STM32H753xx)
#include "stm32h7xx_hal.h"
#endif


void JumpToBootloader(void);
void checkBootloader(char * inputBuffer);

#endif /* INC_JUMPTOBOOTLOADER_H_ */


