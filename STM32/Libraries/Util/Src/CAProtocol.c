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
    // Nothing for now.
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
        idx++;
        if (sscanf(idx, "%d,%lf,%lf", &port, &alpha, &beta) == 3)
        {
            cal[noOfCalibrations] = (CACalibration) { port, alpha, beta };
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

void logging(CAProtocolCtx* ctx, const char *input)
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

static void otp_write(CAProtocolCtx* ctx, const char *input)
{
    uint32_t  OTPVersion;
    uint32_t  BoardType;
    uint32_t  PCBversion[2];
    uint32_t date;

    if (sscanf(input, "OTP w %02lu %02lu %02lu.%02lu %lu", &OTPVersion, &BoardType, &PCBversion[1], &PCBversion[0], &date) == 4)
    {
        // Verify all entries is valid exept date since this is uint32_t.
        if (OTPVersion == OTP_VERSION && BoardType < 0xFF && PCBversion[1] <= 0xFF && PCBversion[0] <= 0xFF)
        {
            // During write only the current version is supported.
            BoardInfo info;
            info.v1.otpVersion = OTP_VERSION;
            info.v1.boardType = BoardType & 0xFF;
            info.v1.pcbVersion.major = PCBversion[1] & 0xFF;
            info.v1.pcbVersion.minor = PCBversion[0] & 0xFF;
            info.v1.productionDate = date;
            ctx->otpWrite(&info);
            return;
        }
    }

    // Invalid format wrong.
    ctx->undefined(input);
}

void inputCAProtocol(CAProtocolCtx* ctx, const char *input)
{
    if (input[0] == '\0') {
        return; // Null terminated string.
    }
    else if(strcmp(input, "Serial") == 0)
    {
        if (ctx->printHeader)
            ctx->printHeader();
    }
    else if(strcmp(input, "DFU") == 0)
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
    else if (strncmp(input, "OTP", 3 == 0))
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
    else if (ctx->undefined)
    {
        ctx->undefined(input);
    }
}

void initCAProtocol(CAProtocolCtx* ctx)
{
    ctx->data = NULL; // no data for now, when needed => malloc(sizeof(CAProtocolData));
}
