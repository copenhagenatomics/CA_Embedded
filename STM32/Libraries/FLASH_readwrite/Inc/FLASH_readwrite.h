/*
Library:			Read and write FLASH memory on STM32F411
Description:		Provides an interface to read and write FLASH memory.
					Tested with STM32F411CE
*/

#ifndef INC_FLASH_READWRITE_H_
#define INC_FLASH_READWRITE_H_

#include <stdint.h>

void writeToFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeSaved);
void readFromFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeRead);

#endif /* INC_FLASH_READWRITE_H_ */

