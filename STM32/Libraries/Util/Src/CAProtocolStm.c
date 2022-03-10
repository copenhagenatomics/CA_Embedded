#include <string.h>
#include "stm32f4xx_hal.h"
#include "CAProtocolStm.h"
#include "usb_device.h"
#include "USBprint.h"
#include "usb_cdc_fops.h"
#include "jumpToBootloader.h"
#include "HAL_otp.h"
#include "systemInfo.h"
#include "time32.h"

void HALundefined(const char *input)
{
    if(strcmp(input, "\0"))
    {
        USBnprintf("MISREAD: %s", input);
    }
}

void HALJumpToBootloader()
{
    // Do not perform the JumpToBootloader now, leave it to CAonBoot
    NVIC_SystemReset();
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

void CAhandleUserInputs(CAProtocolCtx* ctx, const char* startMsg)
{
    static bool isFirstWrite = true;
    char inputBuffer[CIRCULAR_BUFFER_SIZE];

    if (isComPortOpen())
    {
        // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
        if (isFirstWrite)
        {
            USBnprintf(startMsg);
            usb_cdc_rx_flush();
            isFirstWrite = false;
        }
    }
    else
    {
        isFirstWrite = true;
    }

    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(ctx, inputBuffer);
}

static void checkyEnter(const char* inputString)
{
    // User has pressed enter, Jump to DFU mode.
    if (strncmp("y", inputString, 1) == 0)
    {
        USBnprintf("Entering bootloader mode");
        HAL_Delay(200);
        JumpToBootloader(); // function never returns.
    }

    USBnprintf("Misread %s\r\n", inputString);
}

const char* CAonBoot(WWDG_HandleTypeDef *hwwg)
{
    static char msg[100]; // Make static to prevent allocation on stack.

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
    {
        sprintf(msg, "reconnected Reset Reason: Hardware Watch dog");
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    {
        // SW reset. This is set if user has entered DFU and forced a SW reset.
        // User needs to confirm that this is required.
        CAProtocolCtx caProto =
        {
                .undefined = checkyEnter,
                .printHeader = CAPrintHeader,
                .jumpToBootLoader = NULL,
                .calibration = NULL,
                .calibrationRW = NULL,
                .logging = NULL,
                .otpRead = CAotpRead,
                .otpWrite = NULL
        };
        MX_USB_DEVICE_Init();
        initCAProtocol(&caProto);

        uint32_t tStamp = HAL_GetTick();
        while (tdiff_u32(HAL_GetTick(), tStamp) < 30000)
        {
            CAhandleUserInputs(&caProto, "DFU mode: press y<enter> to start SW update");
        }

        // User has done nothing => generate a HW Watch dog reset.
        hwwg->Instance = WWDG;
        hwwg->Init.Prescaler = WWDG_PRESCALER_8;
        hwwg->Init.Window = 127;
        hwwg->Init.Counter = 127;
        hwwg->Init.EWIMode = WWDG_EWI_DISABLE;
        HAL_WWDG_Init(hwwg);

        while(1) {}; // loop until hwwg reset is triggered
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
    {
        sprintf(msg, "reconnected Reset Reason: Power On");
    }
    else
    {
        // System has none of watchdog, SW reset or porrst bit set. At least
        // one reason should be set => this should never happen, inspect!!
        sprintf(msg, "reconnected Reset Reason: Unknown(%lX)", RCC->CSR);
    }

    // Reset the boot flags.
    __HAL_RCC_CLEAR_RESET_FLAGS();

    return msg;
}
