/*
 * activeHeatCtrl.c
 *
 *  Created on: 19 Oct 2022
 *      Author: matias
 */

#include "AD5227x.h"
#include "activeHeatCtrl.h"
#include "ADCMonitor.h"
#include <USBprint.h>
#include <FLASH_readwrite.h>
#include <stdio.h>
#include <string.h>

/***************************************************************************************************
** PRIVATE OBJECTS
***************************************************************************************************/

static float Rtarget[2] = {RTARGET_DEFAULT, RTARGET_DEFAULT};	// 2.8*R(25C) which is equivalent to a sensor temperature of 580 degrees C.
static float Rcurrent[2] = {0};

static bool initHeatingComplete = false;
static int heatingCompleted[2] = {0};
static int isHeating[2] = {0};

static float tempFactor = 2.8;

StmGpio heaterCtrl[NO_HEATERS];
static PortCfg heatCfgs[NO_HEATERS];

static PinGainCtrl heaters[NO_HEATERS] =
{
    { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS1_3_GPIO_Port, CS1_3_Pin, &heatCfgs[0]},
    { CLK_GPIO_Port, CLK_Pin, UD_Pin, CS2_3_GPIO_Port, CS2_3_Pin, &heatCfgs[1]}
};

/***************************************************************************************************
** PRIVATE FUNCTIONS
***************************************************************************************************/

/*!
** @brief Converts ADC measurements to voltage 
*/
static double ADCToVoltage(double adcMean)
{
    return HEATER_V_SCALAR * adcMean + HEATER_V_BIAS;
}

/*!
** @brief Converts ADC measurements to current 
*/
static double ADCToCurrent(double adcMean)
{
    return HEATER_I_SCALAR * adcMean + HEATER_I_BIAS;
}

/***************************************************************************************************
** PUBLIC FUNCTIONS
***************************************************************************************************/

/*!
** @brief Updates the cold resistance value of a sensors
** @note  This is done as part of the initial sensor calibration and is called through
**        the serial interface in oxygen.c
*/
void updateRCold(int channel, float rCold)
{
    if (channel < 0 || channel >= NO_HEATERS)
        return;

    // RCold is 3.25 ± 0.20 Ohm from factory
    if (rCold > 3.45 || rCold < 3.05)
        return;

    Rtarget[channel] = rCold * tempFactor;
}

/*!
** @brief Updates the target resistance of a sensor
*/
void updateRTarget(int channel, float rTarget)
{
    if (channel < 0 || channel >= NO_HEATERS)
        return;

    Rtarget[channel] = rTarget;
}

/*!
** @brief Updates the temperature factor. 
** @note  Default is 2.8
*/
void updateTempFactor(float factor)
{
    if (factor < 2.7 || factor > 2.8)
        return;
        
    tempFactor = factor;
}

/*!
** @brief Starts the heating routine for a sensor. 
*/
void turnOnHeating(int channel)
{
    // Turn on heating control
    isHeating[channel] = 1;
    stmSetGpio(heaterCtrl[channel], true);
}

/*!
** @brief Stops the heating routine for a sensor. 
** @note  The heater_ctrl GPIO is not pulled low until the potentiometer controlling
**        the heater has ramped to its lowest gain step.
*/
void turnOffHeating(int channel)
{
    isHeating[channel] = 0;
    heatingCompleted[channel] = 0;
}

/*!
** @brief Returns whether a sensor has reached its target resistance value. 
*/
int isSensorHeated(int channel)
{
    return heatingCompleted[channel];
}

/*!
** @brief Starts the heating routine for both sensors. 
*/
void enableHeaters()
{
    for (int i = 0; i < NO_HEATERS; i++)
    {
        turnOnHeating(i);
    }
}

/*!
** @brief Stops the heating routine for both sensors and resets the heater routine
**        by setting the initHeatingComplete variable to false.
*/
void disableAndResetHeaters()
{
    initHeatingComplete = false;
    for (int i = 0; i < NO_HEATERS; i++)
    {
        turnOffHeating(i);
    }
}

/*!
** @brief Returns whether the initial heating of the sensors has completed.
*/
int isInitHeatingCompleted()
{
    if (!initHeatingComplete && heatingCompleted[0] && heatingCompleted[1])
    {
        initHeatingComplete = true;
    }
    return initHeatingComplete;
}

/*!
** @brief Returns the target resistances for the sensors.
*/
void getRTarget(float *rChannel1, float *rChannel2)
{
    *rChannel1 = Rtarget[0];
    *rChannel2 = Rtarget[1];
}

/*!
** @brief Returns the current resistances of the sensor heaters.
*/
void getRCurrent(float *rChannel1, float *rChannel2)
{
    *rChannel1 = Rcurrent[0];
    *rChannel2 = Rcurrent[1];
}

/*!
** @brief Main heating control routine.
** @note  This is called through the ADC callback in oxygen.c
*/
void activeHeatControl(int16_t *pData, int noOfChannels, int noOfSamples)
{
    for (int i = 0; i < NO_HEATERS; i++)
    {
        PinGainCtrl *pCtrl = &heaters[i];
        int currentGain = pCtrl->cfg->gain;

        double Iheater = ADCToCurrent(ADCMean(pData, i+2));
        double Vheater = ADCToVoltage(ADCMean(pData, i+4));
        double Rheater = 0;

        // Avoid divide by 0 error (should never occur)
        if (Iheater != 0)
        {
            Rheater = Vheater/Iheater;
        } 
        Rcurrent[i] = Rheater;

        // Turn down Voltage if MAX_A  is superseded or heating control is turned off
        if (isHeating[i] == 0 || Iheater > MAX_A)
        {
            // Turn down heater gain
            int newGain = pCtrl->cfg->gain + 1;
            if (newGain <= MAX_GAIN && newGain >= 0)
            {
                // NOTE: A higher gain step will yield a lower voltage! (and vice-versa).
                pCtrl->cfg->gain++;
            }
            else if (newGain > MAX_GAIN)
            {
                stmSetGpio(heaterCtrl[i], false);
                // Cold resistance is best estimate as the resistance cannot be measured when the heaters are off
                Rcurrent[i] = Rtarget[i]/2.8; 
            }
            setGain(pCtrl, currentGain);
            continue;
        }

        // If target resistance is reached continue 
        if (fabs(Rtarget[i] - Rheater) < 0.02)
        {
            heatingCompleted[i] = 1;
            continue;
        }

        int newGain = (Rtarget[i] > Rheater) ? pCtrl->cfg->gain - 1 : pCtrl->cfg->gain + 1;
        if (newGain <= MAX_GAIN && newGain >= 0)
        {
            pCtrl->cfg->gain = newGain;
            setGain(pCtrl, currentGain);
        }
        else
        {
            pCtrl->cfg->gain = currentGain;
        }
    }
}

/*!
** @brief Initial heater setup
*/
void heatControlInit(ZrO2Device * dev1, ZrO2Device * dev2)
{
    ZrO2Device * devs[2] = {dev1, dev2};
    for (int channel = 0; channel < NO_HEATERS; channel++)
    {
        // Set gain to 0
        PinGainCtrl *pin = &heaters[channel];
        pin->cfg->gain = MAX_GAIN;
        initGain(pin);

        // Turn on heater
        heaterCtrl[channel] = devs[channel]->heaterCtrl;
        turnOnHeating(channel);
    }
}
