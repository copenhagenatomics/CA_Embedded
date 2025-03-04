/*
 * ZrO2Sensor.h
 *
 *  Created on: May 24, 2022
 *      Author: matias
 */

#ifndef INC_ZRO2SENSOR_H_
#define INC_ZRO2SENSOR_H_

#include <StmGpio.h>
#include <stdbool.h>

/***************************************************************************************************
** DEFINES
***************************************************************************************************/

#define ADC_TURNOFF 4090

#define HIGH_RANGE_MAX_LEVEL    250000 // Levels are in ppm
#define HIGH_RANGE_MIN_LEVEL    1000   // Shut off level if low range is within range and stabilised
#define HIGH_TRANSITION_LEVEL   5000  // Point at which the low range sensor is turned on. This is relatively far from the low range area, as the high range can get 'stuck'
#define LOW_RANGE_MAX_LEVEL	    1200	// Shut off level if high range is within range and stabilised
#define LOW_RANGE_MIN_LEVEL	    10     // Shut off level - will only be on periodically
#define LOW_TRANSITION_LEVEL    1000   // Point at which the high range sensor is turned on.

#define HIGH_RANGE_QUAD		    -0.00496
#define HIGH_RANGE_SCALAR	    81.7
#define HIGH_RANGE_BIAS		    -1500
#define LOW_RANGE_QUAD		    0
#define LOW_RANGE_SCALAR	    0.305
#define LOW_RANGE_BIAS		    0

#define STABILIZATION_PERIOD    20000 // 20 seconds (ms)
#define SENSOR_CHECK_INTERVAL   900000 // 15 minutes

/***************************************************************************************************
** SENSOR OBJECTS
***************************************************************************************************/

typedef enum
{
    WITHIN_RANGE,
    BELOW_RANGE,
    ABOVE_RANGE
} ZrO2State;

typedef enum
{
    LOW_RANGE,
    HIGH_RANGE
} ZrO2Type;


typedef struct ZrO2Device {
    StmGpio sensorCtrl;			// Pin controlling current through sensor
    StmGpio heaterCtrl;			// Pin controlling sensor heater circuit

    float minLevel;				// Minimum oxygen level in operating range
    float maxLevel;				// Maximum oxygen level in operating range

    float ADCToOxygenQuad;		// ADC conversion quad
    float ADCToOxygenScalar;	// ADC conversion scalar
    float ADCToOxygenBias;		// ADC conversion bias

    unsigned long offTimer;		// Timestamp to determine whether sensor should be shut off
    unsigned long onTimer;      // Timestamp to determine whether sensor is stabilised
    bool isInTransition;        // Is set when sensor crosses its *_TRANSITION_LEVEL
    bool hasStabilised;         // Is set when sensor has been on for STABILIZATION_PERIOD

    ZrO2Type type;				// 25% or 1000ppm
    ZrO2State state;			// General state of sensor
} ZrO2Device;

/***************************************************************************************************
** PUBLIC FUNCTION PROTOTYPES
***************************************************************************************************/

int isSensorOn(int channel);
double ADCtoOxygen(ZrO2Device * ZrO2, double adc_val);
double computeCombinedOxygen(ZrO2Device * ZrO2High, ZrO2Device * ZrO2Low, double oxygenHigh, double oxygenLow);
void calibrateSensorSlope(ZrO2Device * ZrO2, double oxygenLevel, double trueOxygenLevel);
void updateSensorState(ZrO2Device * ZrO2, double oxygenLevel, double adcMean);
void turnOffSensor(ZrO2Device * ZrO2, int channel);
void turnOnSensor(ZrO2Device * ZrO2, int channel);
void ZrO2SensorInit(ZrO2Device * ZrO2High, ZrO2Device *ZrO2Low);
void setSensorCalibration(ZrO2Device * ZrO2, char sensorType);

#endif /* INC_ZRO2SENSOR_H_ */
