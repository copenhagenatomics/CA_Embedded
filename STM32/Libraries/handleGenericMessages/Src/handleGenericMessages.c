/*
 * serialRead.c
 *
 *  Created on: May 3, 2021
 *      Author: alema
 */

#include <string.h>
#include "stm32f4xx_hal.h"
#include "jumpToBootloader.h"
#include "USBprint.h"
#include "handleGenericMessages.h"

__weak void printHeader() {};

void handleGenericMessages(char * inputBuffer)
{
    if(!(strcmp(inputBuffer, "Serial")))
    {
        printHeader();
    }
    else if(!(strcmp(inputBuffer, "DFU")))
    {
        USBnprintf("Entering bootloader mode");
        HAL_Delay(200);
        JumpToBootloader();
    }
    else if((strcmp(inputBuffer, "\0")))
    {
        USBnprintf("MISREAD: %s", inputBuffer);
    }
}
