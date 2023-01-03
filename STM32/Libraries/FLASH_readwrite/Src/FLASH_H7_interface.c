/*
 * FLASH_H7_interface.c
 *
 *  Created on: 22 Dec 2022
 *      Author: matias
 */

/*
 *  Library:			Read and write FLASH memory on STM32H7xx
 *  Description:		Provides an interface to read and write FLASH memory.
*/

/* TODO: 1. Check the protection status of a FLASH sector before attempting to perform a write instruction. */

// Only include content of file if project targets an STM32H7
#if defined(STM32H753xx)

#include "stm32h7xx_hal.h"
#include <FLASH_H7_interface.h>
#include <string.h>

// Relevant sectors can be found in RM0433 Table 15
#define START_ADDRESS_BANK1	0x08000000
#define END_ADDRESS_BANK1	0x080FFFFF
#define START_ADDRESS_BANK2 0x08100000
#define END_ADDRESS_BANK2 	0x081FFFFF

#define FLASH_WORD 				32 // 32 bytes
#define DATA_ADDRESS_INCREMENT	8

int flashSectorSize = 131072; // Each FLASH sector is 128kB (in binary)

static uint32_t getSector(uint32_t address)
{
	uint32_t denominator = 0x00;
	// Determine which memory bank address lies within
	if (address > START_ADDRESS_BANK1 && address < END_ADDRESS_BANK1)
		denominator = START_ADDRESS_BANK1;
	else if (address > START_ADDRESS_BANK2 && address < END_ADDRESS_BANK2)
		denominator = START_ADDRESS_BANK2;
	else
		return -1;

	uint32_t sectorAddress = address-denominator;
	int sector = sectorAddress/flashSectorSize;
	return sector;
}

uint32_t eraseSectors(uint32_t startAddress, uint16_t size)
{
	static FLASH_EraseInitTypeDef EraseInitStruct;
	uint32_t SECTORError;

	// Unlock the FLASH to enable the flash control register access
	HAL_FLASH_Unlock();

	// Get the number of sector to erase from 1st sector
	uint32_t startSector = getSector(startAddress);
	uint32_t endSectorAddress = startAddress + size*4;
	uint32_t endSector = getSector(endSectorAddress);

	// Check that erase sector is valid
	if (startSector == -1 || endSector == -1)
		return -1;

	// Fill EraseInit structure
	EraseInitStruct.TypeErase     = FLASH_TYPEERASE_SECTORS;
	EraseInitStruct.VoltageRange  = FLASH_VOLTAGE_RANGE_3;
	EraseInitStruct.Sector        = startSector;

	// Find the start memory bank to erase
	if (startAddress < START_ADDRESS_BANK2 && endSectorAddress < START_ADDRESS_BANK2)
		EraseInitStruct.Banks       = FLASH_BANK_1;
	else if (startAddress > START_ADDRESS_BANK2)
		EraseInitStruct.Banks    	= FLASH_BANK_2;
	else EraseInitStruct.Banks    	= FLASH_BANK_BOTH;

	EraseInitStruct.NbSectors     = (endSector - startSector) + 1;

	// Erase the specified user FLASH area
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &SECTORError) != HAL_OK)
	{
		return HAL_FLASH_GetError();
	}
	HAL_FLASH_Lock();
	return 0;
}

static uint32_t programSector(uint32_t startAddress, uint32_t *data, uint16_t size)
{
	HAL_FLASH_Unlock();
	int dataIdx = 0;
	for(uint32_t i=0; i<size; i++)
	{
		if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, startAddress, (uint32_t ) &data[dataIdx]) != HAL_OK)
		{
			// Error occurred while writing data in FLASH memory
			return HAL_FLASH_GetError();
		}
		startAddress += FLASH_WORD;
		dataIdx += DATA_ADDRESS_INCREMENT;
	}
	HAL_FLASH_Lock();
	return 0;
}

// Write data to FLASH_ADDR+indx directly. This method does not
// ensure any data integrity as data validation is performed.
uint32_t writeToFlashH7(uint32_t startAddress, uint32_t *data, uint16_t size)
{
    __HAL_RCC_WWDG_CLK_DISABLE();

    // Erase the sector before write
    if (eraseSectors(startAddress, size) != 0)
    	return -1;

    // Write to sector
    if (programSector(startAddress, data, size) != 0)
    	return -1;
    __HAL_RCC_WWDG_CLK_ENABLE();
    return 0;
}

// Read from flash without checking data integrity if CRC module
// is not enabled in project.
void readFromFlashH7(uint32_t startAddress, uint32_t *data, uint16_t size)
{
    for(uint32_t i=0; i<size; i++)
        *(data + i) = *((uint32_t *)(startAddress+i*4));
}


// If this check is not included the compiler will throw an
// error for projects where CRC module is not enabled
#ifdef HAL_CRC_MODULE_ENABLED
uint32_t computeCRCH7(CRC_HandleTypeDef *hcrc, uint32_t *data, uint16_t size)
{
    // Convert data to uint32_t as needed for the CRC computation
	uint32_t crcData[size];
    memcpy(&crcData, data, size);

    // Compute CRC of stored data
    uint32_t crcVal = HAL_CRC_Calculate(hcrc, crcData, sizeof(crcData)/sizeof(uint32_t));
    return crcVal;
}

// Write data to FLASH_ADDR+indx including CRC.
uint32_t writeToFlashCRC(CRC_HandleTypeDef *hcrc, uint32_t startAddress, uint32_t *data, uint16_t size)
{
    __HAL_RCC_WWDG_CLK_DISABLE();

    // Erase the sector before write
    if (eraseSectors(startAddress, size) != 0)
    	return -1;

    // Save data supplied as input
    if (programSector(startAddress, data, size) != 0)
    	return -1;

    // Compute CRC of data to be saved.
    uint32_t crcVal = computeCRCH7(hcrc, data, size);
    // Save CRC value
    if (programSector(startAddress+size, &crcVal, 1) != 0)
    	return -1;

    __HAL_RCC_WWDG_CLK_ENABLE();
    return 0;
}


// Read from flash without checking data integrity if CRC module
// is not enabled in project.
void readFromFlashCRC(CRC_HandleTypeDef *hcrc, uint32_t startAddress, uint32_t *data, uint16_t size)
{
    // Read data including CRC value
    for(uint32_t i=0; i<size; i++)
        *(data + i) = *( (uint32_t *)(startAddress+i) );

    // Compute CRC of data read.
    uint32_t crcVal = computeCRCH7(hcrc, data, size);

    // Retrieve stored CRC value in flash.
    uint32_t crcStored = 0;
    memcpy(&crcStored, (uint32_t *)(startAddress+size), sizeof(uint32_t));

    // If computed and stored CRC value does not match
    // set data[0]=0xFF which resembles a clean FLASH i.e.
    // default values are loaded and stored in boards.
    if (crcStored != crcVal)
    	*data = 0xFFFFFFFF;
}
#endif
#endif
