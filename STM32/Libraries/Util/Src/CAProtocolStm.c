#include <string.h>
#include "CAProtocolStm.h"
#include "USBprint.h"
#include "jumpToBootloader.h"
#include "HAL_otp.h"
#include "systemInfo.h"

void HALundefined(const char *input)
{
    if(strcmp(input, "\0"))
    {
        USBnprintf("MISREAD: %s", input);
    }
}

void HALJumpToBootloader()
{
    USBnprintf("Entering bootloader mode");
    HAL_Delay(200);
    JumpToBootloader();
}

void CAPrintHeader()
{
    USBnprintf(systemInfo());
}

void CAotpRead()
{
    BoardInfo info;
    if (HAL_otpRead(&info))
    {
        USBnprintf("OTP: No production available");
    }
    else
    {
        switch(info.otpVersion)
        {
        case OTP_VERSION_1:
            USBnprintf("OTP %u %u %u.%u %u\r\n"
                     , info.otpVersion
                     , info.v1.boardType
                     , info.v1.pcbVersion.major
                     , info.v1.pcbVersion.minor
                     , info.v1.productionDate);
            break;
        case OTP_VERSION_2:
            USBnprintf("OTP %u %u %u %u.%u %u\r\n"
                     , info.otpVersion
                     , info.v2.boardType
                     , info.v2.subBoardType
                     , info.v2.pcbVersion.major
                     , info.v2.pcbVersion.minor
                     , info.v2.productionDate);
            break;
        default:
            USBnprintf("Not supported version %d of OTP data. Update firmware in board.", info.otpVersion);
            break;
        }
    }
}
