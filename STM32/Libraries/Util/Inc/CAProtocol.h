/*
 * CAProtocol.h
 *
 *  Created on: Oct 6, 2021
 *      Author: agp
 */

#ifndef INC_CAPROTOCOL_H_
#define INC_CAPROTOCOL_H_

typedef struct
{
    int port;
    double alpha;
    double beta;
} CACalibration;

typedef struct
{
    // Called if message is not found. Overwrite to get info about invalid input
    void (*undefined)(const char* inputString);

    // system info request using "Serial". These should be overwritten
    void (*printHeader)();
    void (*jumpToBootLoader)();

    // Calibration request. overwrite if calibration is supported by module.
    void (*calibration)(int noOfCalibrations, const CACalibration* calibrations);

    struct CAProtocolData *data; // Private data for CAProtocol.
} CAProtocolCtx;

void inputCAProtocol(CAProtocolCtx* ctx, const char *input);
void initCAProtocol(CAProtocolCtx* ctx);

#endif /* INC_CAPROTOCOL_H_ */
