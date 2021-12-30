/*
 * otp.c
 *
 *  Created on: Dec 30, 2021
 *      Author: agp
 */
#include "otpApp.h"

#include "usb_cdc_fops.h"
#include "systemInfo.h"
#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "USBprint.h"

static void printHeader();

// Local variables
static CAProtocolCtx caProto =
{
        .undefined = HALundefined,
        .printHeader = printHeader,
        .jumpToBootLoader = HALJumpToBootloader,
        .calibration = NULL,
        .calibrationRW = NULL,
        .logging = NULL
};

static void printHeader()
{
    // TODO: fetch product/PCB from OTP.
    char productType[] = "OTP";
    char mcuFamily[] = "STM32F401";
    char pcbVersion[] = "V0.0";

    USBnprintf(systemInfo(productType, mcuFamily, pcbVersion));
}

static void handleUserInputs()
{
    char inputBuffer[CIRCULAR_BUFFER_SIZE];

    usb_cdc_rx((uint8_t*) inputBuffer);
    inputCAProtocol(&caProto, inputBuffer);
}


void otpLoop()
{
    // All functionality is based on user input.
    handleUserInputs();
}
