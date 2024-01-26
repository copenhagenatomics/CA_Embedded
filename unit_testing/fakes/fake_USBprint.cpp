/*!
** @file   fake_USBprint.c
** @author Luke W
** @date   22/01/2024
*/

#include "USBprint.h"
#include <iostream>

/* TODO: Make this Log transmissions to a "log_stdout" file, and "receive" transmissions from a 
** "stdin" file. Writing to a the output log file is pretty easy, but input in a way that mimics 
** the USB link for the CDC will be harder. Perhaps make a new thread that continually checks an 
** input file, then copies the input to the buffer (supplied by usb_cdc_rx) and then calls 
** usb_cdc_tx_available */

int USBnprintf(const char * format, ... )
{
    return 0;
}

ssize_t writeUSB(const void *buf, size_t count)
{
    return 0;
}

size_t txAvailable()
{
    return 0;
}

/*!
** @brief Returns whether the port is active or not
*/
bool isUsbPortOpen()
{
    return false;
}

/*!
** @brief Function to get data from the USB receive buffer
*/
int usbRx(uint8_t* buf)
{
    return 0;
}