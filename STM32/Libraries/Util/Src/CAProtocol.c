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
    else if (ctx->undefined)
    {
        ctx->undefined(input);
    }
}

void initCAProtocol(CAProtocolCtx* ctx)
{
    ctx->data = NULL; // no data for now, when needed => malloc(sizeof(CAProtocolData));
}
