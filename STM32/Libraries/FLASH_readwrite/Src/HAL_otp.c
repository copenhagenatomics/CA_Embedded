/*
 * Description:
 * The OTP section is divided in 16 blocks, each containing 8xuint32, 32 bytes (STM32F401)
 * 32 bytes should be enough to save needed data, also in the future and each block is locked during one write.
 *
 * Reading: procedure is to find the latest locked block and parse the date to a BoardInfo struct.
 *          If no block is locked an error is returned.
 * Writing: Find the next free block and write version info to this block section
 */

// Only include content of file if project targets an STM32F4xx
#if defined(STM32F401xC) || defined(STM32F411xx)

#include <string.h>

#include "HAL_otp.h"
#include "stm32f4xx_hal.h"

// Support for both FLASH reading and writing in both stm32f401 and stm32f411 MCUs.
#if defined(STM32F401xx)
#include "stm32f401xc.h"
#elif defined(STM32F411xx)
#include "stm32f411xe.h"
#endif


// Number of OTP lock bytes. Each lock byte points to a section of 32 bytes, 16*32=512 bytes.
#define OTP_SECTION_SIZE 32
#define OTP_SECTIONS     16

// OPT area starts at FLASH_OTP_BASE and lock bytes is at end of OTP area.
#define OTP_LOCK_BYTES (FLASH_OTP_BASE + 512)
#define OTP_IS_LOCKED  0x00

static int getOptSector()
{
    // Find first written sector (reverse)
    for (int i = (OTP_SECTIONS-1); i >=0; i--)
    {
        volatile uint8_t* p = (uint8_t*) (OTP_LOCK_BYTES + i);
        if (*p == OTP_IS_LOCKED)
            return i;
    }

    // all OTP sectors is free
    return -1;
}

// Read the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_EMPTY and boardInfo will be unchanged.
int HAL_otpRead(BoardInfo *boardInfo)
{
    int sector = getOptSector();
    if (sector < 0)
        return OTP_EMPTY; // Nothing has been written to OTP area.

    // Valid section found. Copy data to pointer since user should NOT get direct access to OTP area.
    void* optArea = (void*) (FLASH_OTP_BASE + sector * OTP_SECTION_SIZE);
    memcpy(boardInfo->data, optArea, sizeof(boardInfo->data));

    return OTP_SUCCESS;
}

// Write BoardInfo to OTP flash memory.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_WRITE_FAIL if all OTP sections is written.
const int HAL_otpWrite(const BoardInfo *boardInfo)
{
    int sector = getOptSector();

    // if negative, all sectors is free, else pick the next.
    sector = (sector < 0) ? 0 : sector+1;

    // Check if the OTP section is within the OTP area.
    if (sector >= OTP_SECTIONS) {
        return OTP_WRITE_FAIL;
    }

    // Check the version of the Board info
    if (boardInfo->otpVersion > OTP_VERSION || boardInfo->otpVersion == 0) {
        return OTP_WRITE_FAIL;
    }

    if(HAL_FLASH_Unlock() != HAL_OK) {
        return OTP_WRITE_FAIL;
    }

    // Write board info to sector using word size.
    for (int i=0; i<sizeof(boardInfo->data)/sizeof(uint32_t); i++)
    {
        uint32_t otp = (FLASH_OTP_BASE + sector * OTP_SECTION_SIZE + i*sizeof(uint32_t));
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, otp, boardInfo->data[i]) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return OTP_WRITE_FAIL;
        }
    }

    // Lock OTP section
    uint32_t lock = (uint32_t) (OTP_LOCK_BYTES + sector);
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, lock, OTP_IS_LOCKED))
    {
        HAL_FLASH_Lock();
        return OTP_WRITE_FAIL;
    }

    HAL_FLASH_Lock();
    return OTP_SUCCESS;
}
#endif
