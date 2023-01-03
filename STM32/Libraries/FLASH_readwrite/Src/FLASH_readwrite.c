
/*
 *  Library:			Read and write FLASH memory on STM32F411
 *  Description:		Provides an interface to read and write FLASH memory.
 *  TODO: This is a very brute force implementation where no 'protocol' is used
 *        to read/write the flash. So flash layout should be defined.
*/
#if defined(STM32F401xC)

#include <stm32f4xx_hal.h>
#include <FLASH_readwrite.h>
#include <string.h>

// NOTE: The flash sector and addr variables originate from the ld linker script.
// The flash is used for program data and this must NOT be cleared. Putting data in
// linker script will ensure that program data will not be cleared when writing
// calibration data to the flash. To access these variables the syntax below is used.
extern uint32_t _FlashSector; // Variable defined in ld linker script.
extern uint32_t _FlashAddr;   // Variable defined in ld linker script.
#define FLASH_SECTOR ((uint32_t) &_FlashSector)
#define FLASH_ADDR   ((uint32_t) &_FlashAddr)

// Write data to FLASH_ADDR+indx directly. This method does not
// ensure any data integrity as data validation is performed.
void writeToFlash(uint32_t indx, uint32_t size, uint8_t *data)
{
    __HAL_RCC_WWDG_CLK_DISABLE();

    // Erase the sector before write
    HAL_FLASH_Unlock();
    FLASH_Erase_Sector(FLASH_SECTOR, FLASH_VOLTAGE_RANGE_3);
    HAL_FLASH_Lock();

    // Write to sector
    HAL_FLASH_Unlock();

    for(uint32_t i=0; i<size; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_ADDR+indx+i , data[i]);
    }
    HAL_FLASH_Lock();
    __HAL_RCC_WWDG_CLK_ENABLE();
}

// Read from flash without checking data integrity if CRC module
// is not enabled in project.
void readFromFlash(uint32_t indx, uint32_t size, uint8_t *data)
{
    for(uint32_t i=0; i<size; i++)
    {
        *(data + i) = *( (uint8_t *)(FLASH_ADDR+indx+i) );
    }
}


// If this check is not included the compiler will throw an
// error for projects where CRC module is not enabled
#ifdef HAL_CRC_MODULE_ENABLED
uint32_t computeCRC(CRC_HandleTypeDef *hcrc, uint32_t size, uint8_t *data)
{
    // Convert data to uint32_t as needed for the CRC computation
	uint32_t crcData[size/sizeof(uint32_t)];
    memcpy(&crcData, data, size);

    // Compute CRC of stored data
    uint32_t crcVal = HAL_CRC_Calculate(hcrc, crcData, sizeof(crcData)/sizeof(uint32_t));
    return crcVal;
}

// Write data to FLASH_ADDR+indx including CRC.
void writeToFlashSafe(CRC_HandleTypeDef *hcrc, uint32_t indx, uint32_t size, uint8_t *data)
{
    __HAL_RCC_WWDG_CLK_DISABLE();

    // Erase the sector before write
    HAL_FLASH_Unlock();
    FLASH_Erase_Sector(FLASH_SECTOR, FLASH_VOLTAGE_RANGE_3);
    HAL_FLASH_Lock();
    __HAL_RCC_WWDG_CLK_ENABLE();

    // Compute CRC of data to be saved.
    uint32_t crcVal = computeCRC(hcrc, size, data);

	// Append CRC converted to uint8_t to data
    uint8_t crcData[4] = {0};
	for (uint8_t i = 0; i < 4; i++)
	{
		uint8_t convertedCrcTemp = (crcVal >> 8*i) & 0xFF;
		//*(data + size + i) = convertedCrcTemp;
		crcData[i] = convertedCrcTemp;
	}

	__HAL_RCC_WWDG_CLK_DISABLE();
    // Write to sector
    HAL_FLASH_Unlock();
    // Save data supplied as input
    for(uint32_t i=0; i<size; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_ADDR+indx+i , data[i]);
    }

    // Save CRC value
    for(uint32_t i=0; i<4; i++)
    {
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_ADDR+indx+size+i , crcData[i]);
    }
    HAL_FLASH_Lock();
    __HAL_RCC_WWDG_CLK_ENABLE();
}


// Read from flash without checking data integrity if CRC module
// is not enabled in project.
void readFromFlashSafe(CRC_HandleTypeDef *hcrc, uint32_t indx, uint32_t size, uint8_t *data)
{
    // Read data including CRC value
    for(uint32_t i=0; i<size; i++)
    {
        *(data + i) = *( (uint8_t *)(FLASH_ADDR+indx+i) );
    }

    // Compute CRC of data read.
    uint32_t crcVal = computeCRC(hcrc, size, data);

    // Retrieve stored CRC value in flash.
    uint32_t crcStored = 0;
    memcpy(&crcStored, (uint8_t *)(FLASH_ADDR+indx+size), sizeof(uint32_t));

    // If computed and stored CRC value does not match
    // set data[0]=0xFF which resembles a clean FLASH i.e.
    // default values are loaded and stored in boards.
    if (crcStored != crcVal)
    	*data = 0xFF;
}
#endif
#endif
