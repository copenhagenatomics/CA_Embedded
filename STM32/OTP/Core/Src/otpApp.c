/*!
 * @file    otpApp.c
 * @brief   This file contains the main program of OTP
 * @date    29/10/2026
 * @author  Timothé Dodin
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CAProtocol.h"
#include "CAProtocolStm.h"
#include "HAL_otp.h"
#include "USBprint.h"
#include "otpApp.h"
#include "systemInfo.h"

/***************************************************************************************************
** PRIVATE FUNCTION DECLARATIONS
***************************************************************************************************/

static void otpWrite(const char* input);
static void userInput(const char* input);

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static CAProtocolCtx caProto = {.undefined                = userInput,
                                .printHeader              = CAPrintHeader,
                                .printStatus              = NULL,
                                .printStatusDef           = NULL,
                                .jumpToBootLoader         = HALJumpToBootloader,
                                .calibration              = NULL,
                                .readCalibrationFromRAM   = NULL,
                                .readCalibrationFromFlash = NULL,
                                .writeCalibrationToFlash  = NULL,
                                .logging                  = NULL,
                                .uptime                   = NULL};

/***************************************************************************************************
** PRIVATE FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Parses OTP w command and write production data into OTP
 * @param input Message received
 */
static void otpWrite(const char* input) {
    uint32_t OTPVersion;
    uint32_t BoardType;
    uint32_t SubBoardType;
    uint32_t PCBversion[2];
    uint32_t date;

    // Check for supported version.
    if (sscanf(input, "OTP w %02" PRIu32, &OTPVersion) != 1) {
        return;  // Parse failure, Invalid format of version field.
    }
    if (OTPVersion > OTP_VERSION || OTPVersion == 0) {
        return;  // Not supported version of the OTP data.
    }

    BoardInfo info;
    memset(info.data, 0, sizeof(info.data));

    switch (OTPVersion) {
        case OTP_VERSION_1:
            break;

        case OTP_VERSION_2:
            // During write only the current version is supported.
            if (sscanf(input,
                       "OTP w %02" PRIu32 " %02" PRIu32 " %02" PRIu32 " %02" PRIu32 ".%02" PRIu32
                       " %" PRIu32,
                       &OTPVersion, &BoardType, &SubBoardType, &PCBversion[1], &PCBversion[0],
                       &date) == 6) {
                if (BoardType < 0xFF && PCBversion[1] <= 0xFF && PCBversion[0] <= 0xFF) {
                    info.v2.otpVersion       = OTP_VERSION_2;
                    info.v2.boardType        = BoardType & 0xFF;
                    info.v2.subBoardType     = SubBoardType & 0xFF;
                    info.v2.pcbVersion.major = PCBversion[1] & 0xFF;
                    info.v2.pcbVersion.minor = PCBversion[0] & 0xFF;
                    info.v2.productionDate   = date;

                    if (HAL_otpWrite(&info)) {
                        USBnprintf("OTP: Failed to write productions data\r\n");
                    }
                    return;
                }
            }
            break;

        default:
            break;
    }

    // Invalid input format.
    HALundefined(input);
}

/*!
 * @brief Implementation of the user input
 * @param input Message received
 */
static void userInput(const char* input) {
    if (strncmp(input, "OTP w", 5) == 0) {
        otpWrite(input);
    }
    else {
        HALundefined(input);
    }
}

/***************************************************************************************************
** PUBLIC FUNCTION DEFINITIONS
***************************************************************************************************/

/*!
 * @brief Board initialization function
 */
void otpInit() {
    initCAProtocol(&caProto, usbRx);
}

/*!
 * @brief Loop function called in while(1)
 * @param bootMsg Boot message
 */
void otpLoop(const char* bootMsg) {
    (void)CAhandleUserInputs(&caProto, bootMsg);
}
