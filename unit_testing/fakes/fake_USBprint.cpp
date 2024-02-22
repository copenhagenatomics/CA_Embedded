/*!
** @file   fake_USBprint.c
** @author Luke W
** @date   22/01/2024
*/

#include <fstream>
#include <cstdio>
#include <cstdarg>
#include <vector>
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

static ofstream test_out, test_in;
static stringstream test_ss;
static size_t v_idx = 0;
static char     RX_buffer[TX_RX_BUFFER_LENGTH] = {0};
static size_t   rx_len = 0, rx_off = 0;
static bool     connected = false;

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
    size_t len = snprintf(buf, 3, "\r\n");
    len += vsnprintf(&buf[len], TX_RX_BUFFER_LENGTH, format, argptr);

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
    test_out.flush();

    /* Also write the string to the internal copy buffer */
    test_ss.clear();
    test_ss << string((const char *)buf, count);
    
    return count;
}

size_t txAvailable()
{
    return rx_len;
}

/*!
** @brief Returns whether the port is active or not
*/
bool isUsbPortOpen()
{
    return connected;
}

/*!
** @brief Function to get data from the USB receive buffer
*/
int usbRx(uint8_t* buf)
{
    if(rx_len != 0) {
        *buf = RX_buffer[rx_off++];
        rx_len--;

        return 0;
    }
    else {
        return -1;
    }
}

/*!
** @brief Sends data to the USB device
*/
void hostUSBprintf(const char * format, ...)
{
    va_list argptr;
    va_start(argptr, format);

    if(!test_in.is_open()) {
        test_in.open("test_in.txt");
    }

    size_t temp = rx_off + rx_len;
    size_t len = vsnprintf(&RX_buffer[temp], TX_RX_BUFFER_LENGTH - temp, format, argptr);

    test_in.write((const char *)&RX_buffer[temp], len);
    test_in.flush();
    rx_len += len;
    
    va_end(argptr);
}

/*!
** @brief Flushes USB rx buffer
*/
void usbFlush()
{
    rx_off = 0;
    rx_len = 0;
}

/*!
** @brief Acts as the read function for the host (to allow checking what the device sent)
*/
vector<string>* hostUSBread(bool flush)
{
    vector<string>* result = new vector<string>;
    string str;
    while(getline(test_ss, str)) {
        result->push_back(str);
    }

    if(!flush) {
        /* Put everything back into the stream */
        test_ss.clear();
        for(vector<string>::iterator it = result->begin(); it != result->end(); it++) {
            test_ss << *it;
        }
    }

    return result;
}

/*!
** @brief "Connects" the USB cable
*/
void hostUSBConnect()
{
    connected = true;
}

/*!
** @brief "Disconnects" the USB cable
*/
void hostUSBDisconnect()
{
    connected = false;
}
