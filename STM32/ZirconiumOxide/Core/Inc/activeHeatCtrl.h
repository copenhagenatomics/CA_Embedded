/*
 * activeHeatCtrl.h
 *
 *  Created on: 19 Oct 2022
 *      Author: matias
 */

#ifndef INC_ACTIVEHEATCTRL_H_
#define INC_ACTIVEHEATCTRL_H_

#include "StmGpio.h"
#include "math.h"
#include "main.h"
#include "ZrO2Sensor.h"

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define NO_HEATERS 	   	2
#define MAX_A		   	0.5	// Current limitation of 500mA in heating element.

/*   According to the data sheet the temp factor should be at 2.8 times the cold resistance.
 *   However, the manufacturer suggests using 2.7 due to many sensors' heating elements failing.
 */
#define RTARGET_DEFAULT 9.1

#define HEATER_V_SCALAR	0.0011		// MAX V: 4.5 -> 4.5V/4096 = 0.0011
#define HEATER_V_BIAS	0

#define HEATER_I_SCALAR	0.000161	// MAX I: 660mA -> 0.66A/4096 = 0.000161
#define HEATER_I_BIAS	0

/***************************************************************************************************
** PUBLIC FUNCTION PROTOTYPES
***************************************************************************************************/

int isInitHeatingCompleted();
int isSensorHeated(int channel);
void turnOnHeating(int channel);
void turnOffHeating(int channel);
void enableHeaters();
void disableAndResetHeaters();
void updateTempFactor(float factor);
void updateRCold(int channel, float target);
void updateRTarget(int channel, float target);
void getRCurrent(float *rChannel1, float *rChannel2);
void getRTarget(float *rChannel1, float *rChannel2);
void activeHeatControl(int16_t *pData, int noOfChannels, int noOfSamples);
void heatControlInit(ZrO2Device * dev1, ZrO2Device * dev2);

#endif /* INC_ACTIVEHEATCTRL_H_ */
