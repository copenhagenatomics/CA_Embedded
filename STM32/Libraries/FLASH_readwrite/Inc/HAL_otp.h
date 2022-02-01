#pragma once

#include <stdint.h>

// Only defined formats will be supported. If code/version is bumped, it MUST be possible to read
// old data in the OTP area since this is fixed (for ever).
#define OTP_VERSION_1   0x01 // First version of the production data.
#define OTP_VERSION_2   0x02
#define OTP_VERSION     OTP_VERSION_2 // Current version of the OTP writer format.

// Error codes.
#define OTP_SUCCESS         0 // read/write operation success.
#define OTP_EMPTY           1 // if read operation is done
#define OTP_WRITE_FAIL      2 // content could not be written to OTP area.

// Union for all known production data written in OTP.
typedef union BoardInfo
{
    // Version 1 of this OTP data. This version holds version, Boardtype, PCB version and production date.
    uint32_t data[8];   // data in OTP has size 8xint32. read/write all.
    uint8_t otpVersion; // Due to union this shares address with the first field in struct below.

    struct {
        uint8_t  otpVersion;             // Note OTP version must be located at the first byte (due to union type at top)
        uint8_t  boardType;              // BoardType. This is a fixed table of different board types (fixed)
        struct {
            uint8_t major;
            uint8_t minor;
        } pcbVersion;                    // The PCB version during production. This will make it possible to new SW on old boards.format if <major><minor>
        uint32_t productionDate;         // Production date, used to trace when/where stuff was made.
    } v1;

    struct {
        uint8_t  otpVersion;             // Note OTP version must be located at the first byte (due to union type at top)
        uint8_t  boardType;              // BoardType. This is a fixed table of different board types (fixed)
        uint8_t  subBoardType;           // Some boards can have different peripherals soldered directly to the PCB.
        uint8_t  reserved[3];            // Reserved for future use. ProductionDate must start at address modulus 4.
        struct {
            uint8_t major;
            uint8_t minor;
        } pcbVersion;                    // The PCB version during production. This will make it possible to new SW on old boards.format if <major><minor>
        uint32_t productionDate;         // Production date, used to trace when/where stuff was made.
    } v2;
// future versions of the OPT data.
} BoardInfo;

// Read the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_EMPTY and boardInfo will be unchanged.
int HAL_otpRead(BoardInfo *boardInfo);

// write the current content of the OTP data.
// @param boardInfo pointer to struct BoardInfo.
// Return 0 on success else OTP_WRITE_FAIL.
int HAL_otpWrite(const BoardInfo *boardInfo);
