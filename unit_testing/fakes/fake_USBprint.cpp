/*!
** @file   fake_USBprint.c
** @author Luke W
** @date   22/01/2024
*/

#include <fstream>
#include <cstdio>
#include <cstdarg>
#include <sstream>

#include "USBprint.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define TX_RX_BUFFER_LENGTH 1024
#define TEST_OUT_FILENAME   "testout.txt"

/***************************************************************************************************
** NAMESPACES
***************************************************************************************************/

using namespace std;

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static ofstream test_out;
static char     RX_buffer[TX_RX_BUFFER_LENGTH];
static size_t   RX_offset = 0;


/* TODO: Make this Log transmissions to a "log_stdout" file, and "receive" transmissions from a 
** "stdin" file. Writing to a the output log file is pretty easy, but input in a way that mimics 
** the USB link for the CDC will be harder. Perhaps make a new thread that continually checks an 
** input file, then copies the input to the buffer (supplied by usb_cdc_rx) and then calls 
** usb_cdc_tx_available */



int USBnprintf(const char * format, ... )
{
    va_list argptr;
    va_start(argptr, format);

    char buf[TX_RX_BUFFER_LENGTH] = {0};
    size_t len = vsnprintf(&buf[0], TX_RX_BUFFER_LENGTH, format, argptr);

    len = writeUSB(buf, len);

    va_end(argptr);
    return len;
}

ssize_t writeUSB(const void *buf, size_t count)
{
    /* For first time, open a new file */
    if(!test_out.is_open()) {
        test_out.open(TEST_OUT_FILENAME);
    }

    test_out.write((const char *)buf, count);
    
    return count;
}

size_t txAvailable()
{
    return RX_offset;
}

/*!
** @brief Returns whether the port is active or not
*/
bool isUsbPortOpen()
{
    return true;
}

/*!
** @brief Function to get data from the USB receive buffer
*/
int usbRx(uint8_t* buf)
{
    for(int i = 0; i < RX_offset; i++) {
        buf[i] = RX_buffer[i];
    }
    size_t tmp = RX_offset;
    RX_offset = 0;

    return tmp;
}

/*!
** @brief Sends data to the USB device
*/
void hostUSBprintf(const char * format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    size_t len = vsnprintf(&RX_buffer[RX_offset], TX_RX_BUFFER_LENGTH - RX_offset, format, argptr);

    RX_offset += len;
    
    va_end(argptr);
}

/*!
** @brief Flushes USB rx buffer
*/
void usbFlush()
{
    RX_offset = 0;
}

stringstream hostUSBread()
{
    test_out.flush();
    ifstream test_in(TEST_OUT_FILENAME);
    
    stringstream stream;
    stream << test_in.rdbuf();

    test_in.close();

    return stream;
}