/*
 * ZrO2Sensor.c
 *
 *  Created on: May 24, 2022
 *      Author: matias
 */

#include <stdbool.h>
#include "ZrO2Sensor.h"
#include "main.h"
#include "AD5227x.h"

#define SENSORV_GAIN_LOW 13
#define SENSORV_GAIN_HIGH 16
#define NO_SENSOR_CHANNELS 2

static PortCfg sensorCfgs[NO_SENSOR_CHANNELS] =
{
    { 0, 0, 0, 0 },
    { 0, 0, 0, 0 },
};

// Used for adjusting sensor voltage which should be sensor independent
// and therefore only set once upon startup
PinGainCtrl sensorVoltageCtrl[NO_SENSOR_CHANNELS] =
{
        { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS1_1_GPIO_Port, CS1_1_Pin, &sensorCfgs[0] },
        { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS2_1_GPIO_Port, CS2_1_Pin, &sensorCfgs[1] }
};

/*!
** @brief Checks whether sensors is on by fetching its sensor voltage gain level
** @retval 0 if the sensor is off and a positive integer if on (sensor voltage gain step) 
*/
int isSensorOn(int channel)
{
    return sensorVoltageCtrl[channel].cfg->gain;
}

double ADCtoOxygen(ZrO2Device * ZrO2, double adc_val)
{
    return ZrO2->ADCToOxygenQuad * adc_val * adc_val + ZrO2->ADCToOxygenScalar * adc_val + ZrO2->ADCToOxygenBias;
}

// Adjust slope of sensor
void calibrateSensorSlope(ZrO2Device * ZrO2, double oxygenLevel, double trueOxygenLevel)
{
    ZrO2->ADCToOxygenScalar *= trueOxygenLevel/oxygenLevel;
}

// Compute weighted average of oxygen levels in transition area.
// If one of the sensors is turned of simply return the reading
// from the sensor that is switched on.
double computeCombinedOxygen(ZrO2Device * ZrO2High, ZrO2Device * ZrO2Low, double oxygenHigh, double oxygenLow)
{
    // Return minimum trusted level of high range sensor
    // It is still possible to see what the high range sensors actually outputs,
    // but when only looking at the combined output this ensures continuity
    if (oxygenHigh < HIGH_RANGE_MIN_LEVEL && oxygenHigh != -1)
    {
        oxygenHigh = HIGH_RANGE_MIN_LEVEL;
    }

    // If one sensor is turned off return other sensor value
    if (oxygenHigh == -1)
    {
        return oxygenLow;
    }
    else if (oxygenLow == -1)
    {
        return oxygenHigh;
    }

    // If both sensors are on, but one has not yet stabilised return other sensor value
    if (!ZrO2High->hasStabilised)
    {
        return oxygenLow;
    }
    else if (!ZrO2Low->hasStabilised)
    {
        return oxygenHigh;
    }

    // If both sensors are on and stabilised, return the value of the sensor within its operational
    // range. The low range sensor takes precedence in case both report being within their range.
    if (oxygenLow < LOW_TRANSITION_LEVEL && oxygenLow > LOW_RANGE_MIN_LEVEL)
    {
        return oxygenLow;
    }    
    else if (oxygenHigh < HIGH_RANGE_MAX_LEVEL && oxygenHigh > HIGH_RANGE_MIN_LEVEL)
    {
        return oxygenHigh;
    }
    
    // If both sensors are on and stabilised, but outside of their standard operational range 
    // return the sensor value that has been on for the longest (ensure continuity in output)
    uint32_t timestamp = HAL_GetTick();
    if ((timestamp - ZrO2Low->onTimer) > (timestamp - ZrO2High->onTimer))
    {
        return oxygenLow;
    }
    else
    {
        return oxygenHigh;
    }

    // Please compiler - Should not be possible to reach
    return -1;
}

void updateSensorState(ZrO2Device * ZrO2, double oxygenLevel, double adcMean)
{
    if (!ZrO2->hasStabilised)
    {
        if (HAL_GetTick() - ZrO2->onTimer > STABILIZATION_PERIOD)
        {
            ZrO2->hasStabilised = true;
        }
        return;
    }
    
    if (oxygenLevel > ZrO2->maxLevel || adcMean >= ADC_TURNOFF)
    {
        // Only set timer once.
        ZrO2->offTimer = (ZrO2->state != ABOVE_RANGE) ? HAL_GetTick() : ZrO2->offTimer;
        ZrO2->state = ABOVE_RANGE;
        ZrO2->isInTransition = (ZrO2->type == LOW_RANGE) ? true : false;
    }
    else if (oxygenLevel < ZrO2->minLevel)
    {
        ZrO2->offTimer = (ZrO2->state != BELOW_RANGE) ? HAL_GetTick() : ZrO2->offTimer;
        ZrO2->state = BELOW_RANGE;
        ZrO2->isInTransition = (ZrO2->type == HIGH_RANGE) ? true : false;
    }
    else if ((oxygenLevel > LOW_TRANSITION_LEVEL && ZrO2->type == LOW_RANGE) || (oxygenLevel < HIGH_TRANSITION_LEVEL && ZrO2->type == HIGH_RANGE))
    {
        ZrO2->isInTransition = true;
    }
    else if (oxygenLevel > ZrO2->minLevel && oxygenLevel < ZrO2->maxLevel)
    {
        ZrO2->state = WITHIN_RANGE;
        ZrO2->isInTransition = false;
    }
}

// Ramping function for the potentiometer regarding the sensor voltage.
// This avoids the rush-in current that gives an overshoot, when turning on the sensor.
void rampVGain(ZrO2Device * ZrO2, int channel, bool turnOn)
{
    PinGainCtrl *pin = &sensorVoltageCtrl[channel];
    int currentGain = pin->cfg->gain;

    if (turnOn)
        pin->cfg->gain = (ZrO2->type == HIGH_RANGE) ? SENSORV_GAIN_HIGH : SENSORV_GAIN_LOW;
    else
        pin->cfg->gain = 0;

    setGain(pin, currentGain);
}

void turnOffSensor(ZrO2Device * ZrO2, int channel)
{
    ZrO2->isInTransition = false;
    ZrO2->hasStabilised = false;
    // Set sensor voltage at 0V.
    rampVGain(ZrO2, channel, false);
}

void turnOnSensor(ZrO2Device * ZrO2, int channel)
{
    ZrO2->state = WITHIN_RANGE;
    ZrO2->hasStabilised = false;
    ZrO2->onTimer = HAL_GetTick();
    stmSetGpio(ZrO2->sensorCtrl, true);
    rampVGain(ZrO2, channel, true);
}

void setSensorCalibration(ZrO2Device * ZrO2, char sensorType)
{
    if (sensorType == 'L')
    {
        ZrO2->minLevel 			= LOW_RANGE_MIN_LEVEL;
        ZrO2->maxLevel 			= LOW_RANGE_MAX_LEVEL;
        ZrO2->ADCToOxygenQuad 	= LOW_RANGE_QUAD;
        ZrO2->ADCToOxygenScalar = LOW_RANGE_SCALAR;
        ZrO2->type				= LOW_RANGE;
        ZrO2->state   			= ABOVE_RANGE;
    }
    else if (sensorType == 'H')
    {
        ZrO2->minLevel 			= HIGH_RANGE_MIN_LEVEL;
        ZrO2->maxLevel 			= HIGH_RANGE_MAX_LEVEL;
        ZrO2->ADCToOxygenQuad 	= HIGH_RANGE_QUAD;
        ZrO2->ADCToOxygenScalar = HIGH_RANGE_SCALAR;
        ZrO2->type				= HIGH_RANGE;
        ZrO2->state   			= WITHIN_RANGE;
    }
    ZrO2->isInTransition	= false;
    ZrO2->hasStabilised 	= false;
    ZrO2->offTimer			= 0;
    ZrO2->onTimer			= 0;
}

void ZrO2GPIOInit(ZrO2Device *ZrO2Port1, ZrO2Device *ZrO2Port2)
{
    // Initialize sensor measurement circuit and start by not measuring
    stmGpioInit(&ZrO2Port1->sensorCtrl, sense_ctrl_1_GPIO_Port, sense_ctrl_1_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ZrO2Port2->sensorCtrl, sense_ctrl_2_GPIO_Port, sense_ctrl_2_Pin, STM_GPIO_OUTPUT);

    stmGpioInit(&ZrO2Port1->heaterCtrl, heater_ctrl_1_GPIO_Port, heater_ctrl_1_Pin, STM_GPIO_OUTPUT);
    stmGpioInit(&ZrO2Port2->heaterCtrl, heater_ctrl_2_GPIO_Port, heater_ctrl_2_Pin, STM_GPIO_OUTPUT);

    // Turn off sensorCtrl until the sensor voltage has been set to 0V. 
    stmSetGpio(ZrO2Port1->sensorCtrl, false);
    stmSetGpio(ZrO2Port2->sensorCtrl, false);
}

void ZrO2CalibrationInit(ZrO2Device *ZrO2Port1, ZrO2Device *ZrO2Port2)
{
    // Default setup is: sensor 1 = high range, sensor 2 = low range
    setSensorCalibration(ZrO2Port1, 'H');
    setSensorCalibration(ZrO2Port2, 'L');
}

void ZrO2SensorVGainInit(ZrO2Device *ZrO2Port1, ZrO2Device *ZrO2Port2)
{
    for (int i = 0; i < NO_SENSOR_CHANNELS; i++)
        initGain(&sensorVoltageCtrl[i]);

    // Turn on sensorCtrl while both sensors are initially 0V.
    // The sensor voltage should only be increased when the sensors are heated.
    stmSetGpio(ZrO2Port1->sensorCtrl, true);
    stmSetGpio(ZrO2Port2->sensorCtrl, true);
}

void ZrO2SensorInit(ZrO2Device *ZrO2Port1, ZrO2Device *ZrO2Port2)
{
    // Initialise peripheral pins
    ZrO2GPIOInit(ZrO2Port1, ZrO2Port2);

    // Load calibration
    ZrO2CalibrationInit(ZrO2Port1, ZrO2Port2);

    // Set sensor gain to 0 initially
    ZrO2SensorVGainInit(ZrO2Port1, ZrO2Port2);
}
