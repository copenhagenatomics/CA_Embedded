#include <string.h>
#include "CAProtocolStm.h"
#include "USBprint.h"
#include "jumpToBootloader.h"

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
