
/*
Library:			Read and write FLASH memory on STM32F411
Written by:			Alexander Mizrahi-Werner (alexanderMizrahi@copenhagenAtomics.com)
Last modified:		22-03-2021
Description:
					Provides an interface to read and write FLASH memory.
					Tested with STM32F411CE



*/

#ifndef INC_FLASH_READWRITE_H_
#define INC_FLASH_READWRITE_H_



#endif /* INC_FLASH_READWRITE_H_ */
#include "stm32f4xx_hal.h"

void writeToFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeSaved);

void readFromFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeRead);
