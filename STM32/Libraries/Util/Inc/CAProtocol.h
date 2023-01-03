/*
 * CAProtocol.h
 *
 *  Created on: Oct 6, 2021
 *      Author: agp
 */

#ifndef INC_CAPROTOCOL_H_
#define INC_CAPROTOCOL_H_

#include <stdbool.h>
#if defined(STM32F401xC)
#include "HAL_otp.h"
#elif defined(STM32H753xx)
#include "HAL_H7_otp.h"
#endif

typedef struct
{
    int port;
    double alpha;
    double beta;
    int threshold;
} CACalibration;

typedef int (*ReaderFn)(uint8_t* rxBuf);
typedef struct
{
    // Called if message is not found. Overwrite to get info about invalid input
    void (*undefined)(const char* inputString);

    // system info request using "Serial". These should be overwritten
    void (*printHeader)();
    void (*jumpToBootLoader)();

    // Calibration request.
    void (*calibration)(int noOfCalibrations, const CACalibration* calibrations);
    void (*calibrationRW)(bool write);     // Read or write calibration values to flash

    // Data logger request on port (range [1:N]), Zero means stop.
    void (*logging)(int port);

    // OTP read/write. Don't enable write of OTP data in application shipped
    // to customer. The OTP write is a production operation only!
    void (*otpRead)();
    void (*otpWrite)(BoardInfo *boardInfo);

    // All pins should be in state isOn.
    void (*allOn)(bool isOn);
    void (*portState)(int port, bool state, int percent, int duration);

    struct CAProtocolData *data; // Private data for CAProtocol.
} CAProtocolCtx;

void inputCAProtocol(CAProtocolCtx* ctx);
void initCAProtocol(CAProtocolCtx* ctx, ReaderFn fn);
void flushCAProtocol(CAProtocolCtx* ctx);

#endif /* INC_CAPROTOCOL_H_ */
