
/*
 *  Library:			Read and write FLASH memory on STM32F411
 *  Description:		Provides an interface to read and write FLASH memory.
 *  TODO: This is a very brute force implementation where no 'protocol' is used
 *        to read/write the flash. So flash layout should be defined.
*/

#include <FLASH_readwrite.h>
#include <stm32f4xx_hal.h>

// NOTE: The flash sector and addr variables originate from the ld linker script.
// The flash is used for program data and this must NOT be cleared. Putting data in
// linker script will ensure that program data will not be cleared when writing
// calibration data to the flash. To access these variables the syntax below is used.
extern uint32_t _FlashSector; // Variable defined in ld linker script.
extern uint32_t _FlashAddr;   // Variable defined in ld linker script.
#define FLASH_SECTOR ((uint32_t) &_FlashSector)
#define FLASH_ADDR   ((uint32_t) &_FlashAddr)

void writeToFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeSaved)
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
        HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, FLASH_ADDR+indx+i , dataToBeSaved[i]);
    }
    HAL_FLASH_Lock();
    __HAL_RCC_WWDG_CLK_ENABLE();
}

void readFromFlash(uint32_t indx, uint32_t size, uint8_t *dataToBeRead)
{
    for(uint32_t i=0; i<size; i++)
    {
        *(dataToBeRead + i) = *( (uint8_t *)(FLASH_ADDR+indx+i) );
    }
}
