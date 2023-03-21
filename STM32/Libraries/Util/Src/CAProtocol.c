/*
 * CAProtocol.c
 *
 *  Created on: Oct 6, 2021
 *      Author: agp
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "CAProtocol.h"

typedef struct CAProtocolData {
    size_t len;         // Length of current data.
    uint8_t buf[512];   // Buffer for the string fetched from the circular buffer.
    ReaderFn rxReader;  // Reader for the buffer
} CAProtocolData;

#define MAX_NO_CALIBRATION 12
static void calibration(CAProtocolCtx* ctx, const char* input)
{
    char* idx = index(input, ' ');
    CACalibration cal[MAX_NO_CALIBRATION];
    int noOfCalibrations = 0;

    if (!idx) {
        ctx->undefined(input); // arguments.
        return;
    }
    if (idx[1] == 'w' || idx[1] == 'r')
    {
        if (ctx->calibrationRW)
            ctx->calibrationRW(idx[1] == 'w');
        return;
    }

    // Next follow a port,alpha,beta entry
    while (idx != NULL && noOfCalibrations < MAX_NO_CALIBRATION)
    {
        int port;
        double alpha, beta;
        int threshold;
        idx++;
        // Threshold is optional and therefore only 3 values need to be set.
        // For any board that uses the threshold value an additional check should
        // be made before using for any calibration.
        if (sscanf(idx, "%d,%lf,%lf,%d", &port, &alpha, &beta, &threshold) >= 3)
        {
            cal[noOfCalibrations] = (CACalibration) { port, alpha, beta, threshold };
            noOfCalibrations++;
        }
        idx = index(idx, ' '); // get the next space.
    }

    if (noOfCalibrations != 0)
    {
        ctx->calibration(noOfCalibrations, cal);
    }
    else
        ctx->undefined(input);
}

static void logging(CAProtocolCtx* ctx, const char *input)
{
    char* idx = index(input, ' ');
    int port;

    if (!idx) {
        ctx->undefined(input); // arguments.
        return;
    }

    idx++;
    if (sscanf(idx, "p%d", &port) == 1) {
        ctx->logging(port);
    }
    else {
        ctx->undefined(input);
    }
}

#if (OTP_VERSION != OTP_VERSION_2)
#error "Update of CAProtocol required"
#endif
static void otp_write(CAProtocolCtx* ctx, const char *input)
{
    uint32_t  OTPVersion;
    uint32_t  BoardType;
    uint32_t  SubBoardType;
    uint32_t  PCBversion[2];
    uint32_t  date;

    // Check for supported version.
    if (sscanf(input, "OTP w %02lu", &OTPVersion) != 1)
        return; // Parse failure, Invalid format of version field.
    if (OTPVersion > OTP_VERSION || OTPVersion == 0)
        return; // Not supported version of the OTP data.

    BoardInfo info;
    memset(info.data, 0, sizeof(info.data));

    switch(OTPVersion)
    {
    case OTP_VERSION_1:
        break;

    case OTP_VERSION_2:
        // During write only the current version is supported.
        if (sscanf(input, "OTP w %02lu %02lu %02lu %02lu.%02lu %lu", &OTPVersion, &BoardType, &SubBoardType, &PCBversion[1], &PCBversion[0], &date) == 6)
        {
            if (BoardType < 0xFF && PCBversion[1] <= 0xFF && PCBversion[0] <= 0xFF)
            {
                info.v2.otpVersion = OTP_VERSION_2;
                info.v2.boardType = BoardType & 0xFF;
                info.v2.subBoardType = SubBoardType & 0xFF;
                info.v2.pcbVersion.major = PCBversion[1] & 0xFF;
                info.v2.pcbVersion.minor = PCBversion[0] & 0xFF;
                info.v2.productionDate = date;
                ctx->otpWrite(&info);
                return;
            }
        }
        break;

    default:
        break;
    }

    // Invalid input format.
    ctx->undefined(input);
}

static int CAgetMsg(CAProtocolCtx* ctx)
{
    CAProtocolData* protocolData = ctx->data;
    int msgLen = 0;

    while (msgLen == 0)
    {
        uint8_t rxByte;

        if (protocolData->rxReader(&rxByte))
            return 0; // No more data in buffer => no message.

        protocolData->buf[protocolData->len] = rxByte;
        if (rxByte == '\r' || rxByte == '\n')
        {
            if (protocolData->len == 0) {
                continue; // Ignore this zero length message.
            }

            // A valid string is received.
            msgLen = protocolData->len;
            break;
        }

        protocolData->len++; // Continue receive from buffer.
        if (protocolData->len == (sizeof(protocolData->buf) - 1))
        {
            // Buffer overflow!! This is a protocol error or garbage from peer.
            // No string should be bigger then protocolData buffer size.
            // Simulate a string since there might be something of value.
            msgLen = protocolData->len;
            break;
        }
    }

    if (msgLen != 0)
    {
        // enforce zero terminate of string.
        protocolData->buf[protocolData->len] = 0;
        // Reset for receive of a new message. Caller must handle message or save it locally.
        protocolData->len = 0;
    }

    return msgLen;
}

void inputCAProtocol(CAProtocolCtx* ctx)
{
    int msgLen = CAgetMsg(ctx);
    if (msgLen == 0) {
        return; // No message received
    }

    // A message is received i.e. a zero terminated string
    char* input = (char *)ctx->data->buf;

    if(strncmp(input, "Serial", 6) == 0)
    {
        if (ctx->printHeader)
            ctx->printHeader();
    }
    else if(strncmp(input, "DFU", 3) == 0)
    {
        if (ctx->jumpToBootLoader)
            ctx->jumpToBootLoader();
    }
    else if (strncmp(input, "CAL", 3) == 0)
    {
        if (ctx->calibration)
            calibration(ctx, input);
    }
    else if (strncmp(input, "LOG", 3) == 0)
    {
        if (ctx->logging)
            logging(ctx, input);
    }
    else if (strncmp(input, "OTP", 3) == 0)
    {
        if (input[4] == 'r') {
            if (ctx->otpRead)
                ctx->otpRead();
        }
        else if (input[4] == 'w') {
            if (ctx->otpWrite)
                otp_write(ctx, input);
        }
    }
    else if (strncmp(input, "all on", 6) == 0)
    {
        if (ctx->allOn) ctx->allOn(true);
    }
    else if (strncmp(input, "all off", 7) == 0)
    {
        if (ctx->allOn) ctx->allOn(false);
    }
    else if (input[0] == 'p' && strnlen(input, 13) <= 13) // 13 since that is length of pX on YY ZZZ%
    {
        char cmd[13];
        int port;
        /* Valid commands are:
           all on - turn all ports on indefinitely
           all off - turn all ports off
           pX on - turn off port number X
           pX off - turn on port number X indefinitely 'always on'
           pX on YY - turn on port number X for YY seconds
           pX on ZZZ% - turn on port number X on ZZ percent of the time using PWM 'always on'
           pX on YY ZZZ% - turn on port number X for YY seconds ZZ percent of the time using PWM */
        if (sscanf(input, "p%d %[onf]", &port, cmd) != 2)
            ctx->undefined(input);
        if (!ctx->portState)
            return;
        if (strncmp(cmd, "off", 3) == 0) {
            ctx->portState(port, false, 0, -1);
        }
        else if (strncmp(cmd, "on", 2) == 0)
        {
            char *argv[4] = { 0 }; // There should not be more then 4 args.
            const char delim = ' ';
            char *tok = strtok(input, &delim), percent = 0;
            int count=0, tmp;
            for (; count < 4 && tok; count++)
            {
                argv[count] = tok;
                tok = strtok(NULL, &delim);
                if (tok)
                    *(tok-1) = 0; // Zero terminate previous string.
            }
            switch(count)
            {
            case 2: // pX on
                ctx->portState(port, true, 100, -1);
                break;
            case 3: // pX on ZZZ% or YY
                {
                int argc = sscanf(argv[2], "%d%c", &count, &percent);
                if (argc == 2 && percent == '%')
                    ctx->portState(port, true, count, -1);
                else if (argc == 1)
                    ctx->portState(port, true, 100, count);
                else
                    ctx->undefined(input);
                break;
                }
            case 4: // pX on YY ZZZ%
                if (sscanf(argv[2], "%d", &tmp) == 1 && sscanf(argv[3], "%d%c", &count, &percent) == 2 && percent == '%')
                    ctx->portState(port, true, count, tmp);
                else
                    ctx->undefined(input);
                break;
            }
        }
    }
    else if (ctx->undefined)
    {
        ctx->undefined(input);
    }
}

void initCAProtocol(CAProtocolCtx* ctx, ReaderFn fn)
{
    ctx->data = malloc(sizeof(CAProtocolData));
    memset(ctx->data->buf, 0, sizeof(ctx->data->buf));
    ctx->data->len = 0;
    ctx->data->rxReader = fn;
}

void flushCAProtocol(CAProtocolCtx* ctx)
{
    ctx->data->len = 0;
    memset(ctx->data->buf, 0, sizeof(ctx->data->buf));
}
