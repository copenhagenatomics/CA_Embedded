/*
 * Description:
 * The OTP section is divided in 16 blocks, each containing 8xuint32, 32 bytes (STM32F401)
 * 32 bytes should be enough to save needed data, also in the future and each block is locked during one write.
 *
 * Reading: procedure is to find the latest locked block and parse the date to a BoardInfo struct.
 *          If no block is locked an error is returned.
 * Writing: Find the next free block and write version info to this block section
 */

#include "HAL_otp.h"
#include "stm32f4xx_hal.h"

// Read the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_EMPTY and boardInfo will be unchanged.
const int HAL_otpRead(BoardInfo *boardInfo)
{
    // TODO: Implement
    return OTP_EMPTY;
}

// write the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_WRITTEN if OTP data is present.
const int HAL_otpWrite(const BoardInfo *boardInfo)
{
    // TODO: Implement
    return OTP_WRITE_FAIL;
}
