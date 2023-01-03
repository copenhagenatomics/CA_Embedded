/*
Library:			Read and write FLASH memory on STM32F411
Description:		Provides an interface to read and write FLASH memory.
					Tested with STM32F411CE
*/

#ifndef INC_FLASH_READWRITE_H_
#define INC_FLASH_READWRITE_H_

#include <stdint.h>
#if defined(HAL_CRC_MODULE_ENABLED) && defined(STM32F401xC)
  #include "stm32f4xx_hal_crc.h"
#elif defined(HAL_CRC_MODULE_ENABLED) && defined(STM32H753xx)
  #include "stm32h7xx_hal_crc.h"
#endif


void writeToFlash(uint32_t indx, uint32_t size, uint8_t *data);
void readFromFlash(uint32_t indx, uint32_t size, uint8_t *data);

#ifdef HAL_CRC_MODULE_ENABLED
	void writeToFlashSafe(CRC_HandleTypeDef *hcrc, uint32_t indx, uint32_t size, uint8_t *data);
	void readFromFlashSafe(CRC_HandleTypeDef *hcrc, uint32_t indx, uint32_t size, uint8_t *data);
#endif
#endif /* INC_FLASH_READWRITE_H_ */
