/*
 * otp.c
 *
 *  Created on: Dec 30, 2021
 *      Author: agp
 */
#include "otpApp.h"

#include "usb_cdc_fops.h"
#include "systemInfo.h"
#include "HAL_otp.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "USBprint.h"

static void printHeader();
static void otpRead();
static void otpWrite(BoardInfo *boardInfo);

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = printHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL,
        .otpRead  = otpRead,
        .otpWrite = otpWrite
};

static void printHeader()
{
    USBnprintf(systemInfo());
}

static void otpRead()
{
    BoardInfo info;
    if (HAL_otpRead(&info))
    {
        USBnprintf("OTP: No production available");
    }
    else
    {
        USBnprintf("OTP %u %u %u.%u %u\r\n"
                , info.otpVersion
                , info.v1.boardType
                , info.v1.pcbVersion.major
                , info.v1.pcbVersion.minor
                , info.v1.productionDate);
    }
}

static void otpWrite(BoardInfo *boardInfo)
{
    if (HAL_otpWrite(boardInfo)) {
        USBnprintf("OTP: Failed to write productions data");
    }
}

static void clearLineAndBuffer()
{
    // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
    USBnprintf("reconnected");
    usb_cdc_rx_flush();
}

static void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];

    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}

void otpLoop()
{
    static bool isFirstWrite = true;

    // All functionality is based on user input.
    if (isComPortOpen())
    {
        // Upon first write print line and reset circular buffer to ensure no faulty misreads occurs.
        if (isFirstWrite)
        {
            clearLineAndBuffer();
            isFirstWrite = false;
        }
    }
    else {
        isFirstWrite = true;
    }

    handleUserInputs();
}
