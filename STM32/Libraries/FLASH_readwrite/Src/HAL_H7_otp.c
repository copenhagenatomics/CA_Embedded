/*
 * HAL_H7_otp.c
 *
 *  Created on: 28 Dec 2022
 *      Author: matias
 */

// Only include content of file if project targets an STM32H7
#if defined(STM32H753xx)

#include <string.h>
#include <stdbool.h>

#include "HAL_H7_otp.h"
#include "stm32h7xx_hal.h"
#include "stm32h7xx.h"
#include "FLASH_H7_interface.h"

// Section 4 of Memory Bank 1 is write protected by default and is therefore dedicated
// to OTP memory. This part of the memory is therefore not allowed to write to during
// normal FLASH storing.
#define FLASH_OTP_BASE		0x08080000
#define OTP_FLASH_SECTOR	0x04

uint32_t OTP_CLEAR_ADDRESS = 0xFFFFFFFF;

#define FLASH_WORD_SIZE		32 // 32 bytes

// This method can change the write-protection of a sector of BANK 1.
// The documentation for this may be found in reference manual RM0433
// section 4.5.1 and 4.9.16.
static HAL_StatusTypeDef changeWriteProtectedSector(bool enable)
{
	if (!enable && ((FLASH->OPTCR & 0x01) == 0U))
	{
		// Remove write protection of 'OTP' sector
		FLASH->WPSN_PRG1 |= (1UL << OTP_FLASH_SECTOR);

		if (((FLASH->WPSN_PRG1 >> 4) & 0x01) != 0x01)
			return HAL_ERROR;

	}
	else if (enable && ((FLASH->OPTCR & 0x01) == 0U))
	{
		// Enable write protection of 'OTP' sector
		FLASH->WPSN_PRG1 &= ~(1UL << OTP_FLASH_SECTOR);

		if (((FLASH->WPSN_PRG1 >> 4) & 0x01) != 0U)
			return HAL_ERROR;
	}
	return HAL_OK;
}

// Check whether OTP data is already programmed on board.
static int isOTPAvailable()
{
    volatile uint32_t* p = (uint32_t*) FLASH_OTP_BASE;
    if (*p == OTP_CLEAR_ADDRESS)
    	return 0;

    // all OTP sectors is free
    return 1;
}

// Read the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_EMPTY and boardInfo will be unchanged.
int HAL_otpRead(BoardInfo *boardInfo)
{
    if (!isOTPAvailable())
        return OTP_EMPTY; // Nothing has been written to OTP area.

    // Valid section found. Copy data to pointer since user should NOT get direct access to OTP area.
    void* otpArea = (void*) FLASH_OTP_BASE;
    memcpy(boardInfo->data, otpArea, sizeof(boardInfo->data)/sizeof(uint32_t));

    return OTP_SUCCESS;
}

// Write BoardInfo to OTP flash memory.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_WRITE_FAIL if all OTP sections is written.
const int HAL_otpWrite(const BoardInfo *boardInfo)
{
    // Check the version of the Board info
    if (boardInfo->otpVersion > OTP_VERSION || boardInfo->otpVersion == 0) {
        return OTP_WRITE_FAIL;
    }

    // If sector has already been written to erase first.
    if (isOTPAvailable())
    {
    	if (eraseSectors(FLASH_OTP_BASE, sizeof(boardInfo->data)/sizeof(uint32_t)) != 0)
    		return OTP_WRITE_FAIL;
    }

    if (HAL_FLASH_Unlock() != HAL_OK)
        return OTP_WRITE_FAIL;

    // Unlock Flash OPT registry that allows removal of write protection of FLASH section
    if (HAL_FLASH_OB_Unlock() != HAL_OK)
    	return OTP_WRITE_FAIL;

    // Remove write protection of 'OTP' sector
    if (changeWriteProtectedSector(false) != HAL_OK)
    	return OTP_WRITE_FAIL;

    // Write board info to sector using word size.
    uint32_t otpStartAddress = FLASH_OTP_BASE;
    for (int i=0; i<sizeof(boardInfo->data)/sizeof(uint32_t); i++)
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, otpStartAddress, (uint32_t) &boardInfo->data[i]) != HAL_OK)
        {
        	HAL_FLASH_OB_Lock();
            HAL_FLASH_Lock();
            return OTP_WRITE_FAIL;
        }
        otpStartAddress += FLASH_WORD_SIZE;
    }

    if (changeWriteProtectedSector(true) != HAL_OK)
    	return OTP_WRITE_FAIL;

    if (HAL_FLASH_OB_Lock() != HAL_OK)
    	return OTP_WRITE_FAIL;

    HAL_FLASH_Lock();
    return OTP_SUCCESS;
}
#endif
