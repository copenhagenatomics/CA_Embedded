
/*
Library:			Read and write FLASH memory on STM32F411
Written by:			Alexander Mizrahi-Werner (alexanderMizrahi@copenhagenAtomics.com)
Last modified:		22-03-2021
Description:
					Provides an interface to read and write FLASH memory.
					Tested with STM32F411CE

//minimum use case

  HAL_Delay(500); //important! otherwise the program access the flash before the debugger can reset the STM32. This soft bricks the device
  uint8_t dataWrite[4] = {1, 2, 3, 4};
  uint8_t dataRead[4];

  writeToFlash(0,4,dataWrite);
  readFromFlash(0,4,dataRead);

*/

#include "FLASH_readwrite.h"
#include "stm32f4xx_hal.h"


void writeToFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeSaved){
	//sector 5 is the last sector of the memory on F401CC, this such that it will interfere with the main program and vice versa.
	uint8_t sector = 5;
	//Write address within sector 5
	uint32_t flashAddress = 0x08020000;

	//Erase the sector before write
	HAL_FLASH_Unlock();
	FLASH_Erase_Sector(sector, FLASH_VOLTAGE_RANGE_3);
	HAL_FLASH_Lock();

	//Write to sector
	HAL_FLASH_Unlock();
    for(uint32_t i=0; i<size; i++){
    	HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, flashAddress+indx+i , dataToBeSaved[i]);
	}
    HAL_FLASH_Lock();
}

void readFromFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeRead){
	uint32_t flashAddress = 0x08020000;
	for(uint32_t i=0; i<size; i++)
	{
		*((uint8_t *)dataToBeRead + i) = *((uint8_t *)flashAddress+indx+i);
	}
}
