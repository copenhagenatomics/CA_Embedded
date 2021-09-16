/*
 * USBprint.c
 * provides a function similar to the standard printf with capability to print data over USB
 *  Created on: Apr 26, 2021
 *      Author: Alexander.mizrahi@copenhagenatomics.com
 */


#include "USBprint.h"
#include "stdarg.h"
#include "string.h"
#include "stdio.h"
#include "usbd_cdc_if.h"

int USBnprintf(const char * format, ... )
{
    static char buffer[256];
    int len = snprintf(buffer, 3, "\r\n");

    va_list args;
    va_start (args, format);
    len += vsnprintf(&buffer[len], sizeof(buffer) - len, format, args);
    va_end (args);

    CDC_Transmit_FS((uint8_t*)buffer, len);
    HAL_Delay(2);

    return len;
}
